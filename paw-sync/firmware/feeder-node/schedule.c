/*
 * schedule.c — PawSync feeding schedule + weight-loss plan manager
 *
 * Manages feeding schedules (per-pet portions and times) and automatic
 * weight-loss plan adjustments. Schedules are synced from the cloud
 * via the hub, but work offline with cached data.
 *
 * Weight-loss plan:
 *   - Target weight set by vet/owner in app
 *   - Daily calorie target computed from target weight + activity level
 *   - Portions split across scheduled feedings
 *   - Auto-adjust: if pet eats <75% consistently, reduce portion size
 *   - Progress tracked: weekly weight trend from feeder load cell
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdint.h>
#include <string.h>
#include <math.h>
#include "paw_protocol.h"

#define MAX_SCHEDULE_SLOTS  16
#define MAX_PETS             4
#define WEEKLY_REVIEW_DAYS   7

/* ---- Data structures ---- */
typedef struct {
    uint8_t  pet_id;
    uint8_t  hour;
    uint8_t  minute;
    uint16_t portion_g;
    uint8_t  enabled;
} schedule_slot_t;

typedef struct {
    uint8_t  pet_id;
    char     name[16];
    uint16_t target_weight_g;
    uint16_t current_weight_g;
    uint16_t daily_calorie_target;
    uint8_t  weight_loss_mode;     /* 0=maintenance, 1=loss */
    float    activity_multiplier;   /* 1.0=normal, 1.2=active, 0.8=sedentary */
    /* Weekly tracking */
    uint16_t daily_intake_history[7];
    uint8_t  history_day;
} pet_weight_plan_t;

static schedule_slot_t schedule[MAX_SCHEDULE_SLOTS];
static int schedule_count = 0;
static pet_weight_plan_t plans[MAX_PETS];
static int plan_count = 0;

/* ---- Calorie calculation (simplified MER) ---- */
/*
 * Maintenance Energy Requirement (MER):
 *   MER = RER × activity_factor
 *   RER = 70 × (body_weight_kg)^0.75
 *
 * For weight loss: MER = RER × 0.8 (20% deficit)
 */
static uint16_t calculate_daily_calories(float weight_kg, float activity,
                                          uint8_t weight_loss_mode)
{
    float rer = 70.0f * powf(weight_kg, 0.75f);
    float factor = weight_loss_mode ? 0.8f : 1.0f;
    return (uint16_t)(rer * activity * factor);
}

/* ---- Calories to grams conversion ---- */
/*
 * Dry kibble: ~3.5 kcal/g
 * Wet food: ~1.0 kcal/g
 */
static uint16_t calories_to_grams(uint16_t calories, float kcal_per_g)
{
    if (kcal_per_g <= 0) kcal_per_g = 3.5f;  /* default: dry kibble */
    return (uint16_t)(calories / kcal_per_g);
}

/* ---- Schedule management ---- */
int schedule_add_slot(uint8_t pet_id, uint8_t hour, uint8_t minute,
                      uint16_t portion_g)
{
    if (schedule_count >= MAX_SCHEDULE_SLOTS) return -1;
    schedule[schedule_count].pet_id = pet_id;
    schedule[schedule_count].hour = hour;
    schedule[schedule_count].minute = minute;
    schedule[schedule_count].portion_g = portion_g;
    schedule[schedule_count].enabled = 1;
    schedule_count++;
    return 0;
}

int schedule_remove_slot(int slot_idx)
{
    if (slot_idx < 0 || slot_idx >= schedule_count) return -1;
    /* Shift remaining slots */
    for (int i = slot_idx; i < schedule_count - 1; i++)
        schedule[i] = schedule[i + 1];
    schedule_count--;
    return 0;
}

int schedule_get_next(uint8_t current_hour, uint8_t current_minute,
                      schedule_slot_t *next_slot)
{
    int best_idx = -1;
    int best_minutes = 24 * 60;  /* max: 24 hours */

    for (int i = 0; i < schedule_count; i++) {
        if (!schedule[i].enabled) continue;
        int slot_min = schedule[i].hour * 60 + schedule[i].minute;
        int now_min = current_hour * 60 + current_minute;
        int diff = slot_min - now_min;
        if (diff <= 0) diff += 24 * 60;  /* wrap to next day */
        if (diff < best_minutes) {
            best_minutes = diff;
            best_idx = i;
        }
    }

    if (best_idx >= 0 && next_slot) {
        memcpy(next_slot, &schedule[best_idx], sizeof(schedule_slot_t));
        return best_minutes;
    }
    return -1;
}

/* ---- Weight-loss plan management ---- */
int plan_create(uint8_t pet_id, const char *name, uint16_t target_weight_g,
                uint16_t current_weight_g, float activity, uint8_t weight_loss_mode)
{
    if (plan_count >= MAX_PETS) return -1;
    pet_weight_plan_t *p = &plans[plan_count];
    p->pet_id = pet_id;
    strncpy(p->name, name, sizeof(p->name) - 1);
    p->target_weight_g = target_weight_g;
    p->current_weight_g = current_weight_g;
    p->activity_multiplier = activity;
    p->weight_loss_mode = weight_loss_mode;
    p->daily_calorie_target = calculate_daily_calories(
        current_weight_g / 1000.0f, activity, weight_loss_mode);
    memset(p->daily_intake_history, 0, sizeof(p->daily_intake_history));
    p->history_day = 0;
    plan_count++;
    return 0;
}

/* ---- Record daily intake ---- */
void plan_record_intake(uint8_t pet_id, uint16_t consumed_g)
{
    for (int i = 0; i < plan_count; i++) {
        if (plans[i].pet_id == pet_id) {
            plans[i].daily_intake_history[plans[i].history_day] = consumed_g;
            plans[i].history_day = (plans[i].history_day + 1) % 7;
            break;
        }
    }
}

/* ---- Auto-adjust portions based on intake history ---- */
uint16_t plan_adjust_portion(uint8_t pet_id, uint16_t current_portion)
{
    for (int i = 0; i < plan_count; i++) {
        if (plans[i].pet_id != pet_id) continue;
        if (!plans[i].weight_loss_mode) return current_portion;

        /* Check if pet is consistently eating <75% of portions */
        int under_eaten = 0;
        int total_days = 0;
        uint16_t avg_intake = 0;
        for (int d = 0; d < 7; d++) {
            if (plans[i].daily_intake_history[d] > 0) {
                avg_intake += plans[i].daily_intake_history[d];
                total_days++;
                /* Compare to expected daily intake */
                uint16_t expected = plans[i].daily_calorie_target / 3.5f;
                if (plans[i].daily_intake_history[d] < expected * 0.75f)
                    under_eaten++;
            }
        }
        if (total_days == 0) return current_portion;
        avg_intake /= total_days;

        /* If >3 days of under-eating, reduce portion by 10% */
        if (under_eaten >= 3) {
            uint16_t new_portion = (uint16_t)(current_portion * 0.9f);
            return new_portion;
        }
        /* If eating everything and still above target weight,
         * slightly reduce portion */
        if (plans[i].current_weight_g > plans[i].target_weight_g &&
            avg_intake >= current_portion) {
            return (uint16_t)(current_portion * 0.95f);
        }
        return current_portion;
    }
    return current_portion;
}

/* ---- Weekly review ---- */
typedef struct {
    uint8_t  pet_id;
    uint16_t avg_daily_intake;
    uint16_t expected_daily_intake;
    int8_t   weight_change_g;   /* current - last_week */
    uint8_t  on_track;          /* 1 if progressing toward target */
} weekly_review_t;

weekly_review_t plan_weekly_review(uint8_t pet_id)
{
    weekly_review_t review = {0};
    review.pet_id = pet_id;

    for (int i = 0; i < plan_count; i++) {
        if (plans[i].pet_id != pet_id) continue;

        uint16_t total = 0;
        int days = 0;
        for (int d = 0; d < 7; d++) {
            if (plans[i].daily_intake_history[d] > 0) {
                total += plans[i].daily_intake_history[d];
                days++;
            }
        }
        review.avg_daily_intake = (days > 0) ? total / days : 0;
        review.expected_daily_intake = calories_to_grams(
            plans[i].daily_calorie_target, 3.5f);

        /* On track if intake is within 15% of target
         * and weight is moving toward target */
        if (review.avg_daily_intake > 0) {
            float ratio = (float)review.avg_daily_intake /
                          review.expected_daily_intake;
            review.on_track = (ratio > 0.85f && ratio < 1.15f) ? 1 : 0;
        }
        break;
    }
    return review;
}