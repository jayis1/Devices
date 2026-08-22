from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
import pandas as pd
from sklearn.ensemble import GradientBoostingClassifier, IsolationForest, RandomForestClassifier
from sklearn.metrics import classification_report
from sklearn.model_selection import train_test_split


def load_dataset(path: Path, generator):
    if path.exists():
        return pd.read_csv(path)
    return generator()


def readiness_dataset() -> pd.DataFrame:
    rng = np.random.default_rng(11)
    rows = 500
    df = pd.DataFrame({
        'minutes_to_departure': rng.integers(0, 60, rows),
        'backpack_present': rng.integers(0, 2, rows),
        'lunch_packed': rng.integers(0, 2, rows),
        'weather_complexity': rng.uniform(0, 1, rows),
        'interventions': rng.integers(0, 6, rows),
    })
    df['label'] = np.where((df['backpack_present'] == 1) & (df['lunch_packed'] == 1) & (df['minutes_to_departure'] > 8), 'ready', 'risk')
    return df


def lateness_dataset() -> pd.DataFrame:
    rng = np.random.default_rng(12)
    rows = 600
    df = pd.DataFrame({
        'minutes_to_departure': rng.integers(0, 45, rows),
        'child_count': rng.integers(1, 4, rows),
        'weather_complexity': rng.uniform(0, 1, rows),
        'missing_item_count': rng.integers(0, 4, rows),
        'historical_delay_min': rng.uniform(0, 15, rows),
    })
    df['label'] = np.where(df['minutes_to_departure'] - df['missing_item_count'] * 4 - df['historical_delay_min'] > 10, 'on_time', 'late')
    return df


def forgot_dataset() -> pd.DataFrame:
    rng = np.random.default_rng(13)
    rows = 550
    df = pd.DataFrame({
        'minutes_to_departure': rng.integers(0, 40, rows),
        'bag_lifted': rng.integers(0, 2, rows),
        'lunch_packed': rng.integers(0, 2, rows),
        'schedule_change': rng.integers(0, 2, rows),
        'interventions': rng.integers(0, 5, rows),
    })
    df['label'] = np.where((df['bag_lifted'] == 1) & (df['lunch_packed'] == 1) & (df['schedule_change'] == 0), 'low', 'high')
    return df


def lunch_dataset() -> pd.DataFrame:
    rng = np.random.default_rng(14)
    rows = 500
    df = pd.DataFrame({
        'mass_grams': rng.integers(250, 1200, rows),
        'ice_pack_present': rng.integers(0, 2, rows),
        'plate_temp_c': rng.uniform(2, 18, rows),
        'ambient_temp_c': rng.uniform(18, 35, rows),
        'minutes_until_lunch': rng.integers(90, 360, rows),
    })
    df['label'] = np.where((df['ice_pack_present'] == 1) & (df['ambient_temp_c'] < 28) & (df['plate_temp_c'] < 8), 'safe', 'risk')
    return df


def route_dataset() -> pd.DataFrame:
    rng = np.random.default_rng(15)
    rows = 450
    return pd.DataFrame({
        'route_minutes': rng.uniform(5, 45, rows),
        'expected_minutes': rng.uniform(5, 35, rows),
        'boarded': rng.integers(0, 2, rows),
        'child_present': rng.integers(0, 2, rows),
        'backpack_present': rng.integers(0, 2, rows),
    })


def train_classifier(df: pd.DataFrame, feature_cols: list[str], model) -> None:
    X = df[feature_cols]
    y = df['label']
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42, stratify=y)
    model.fit(X_train, y_train)
    pred = model.predict(X_test)
    print(classification_report(y_test, pred))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--task', default='forgot')
    parser.add_argument('--data', type=Path, default=Path('dataset.csv'))
    args = parser.parse_args()

    if args.task == 'readiness':
        df = load_dataset(args.data, readiness_dataset)
        train_classifier(df, ['minutes_to_departure', 'backpack_present', 'lunch_packed', 'weather_complexity', 'interventions'], GradientBoostingClassifier(random_state=42))
    elif args.task == 'lateness':
        df = load_dataset(args.data, lateness_dataset)
        train_classifier(df, ['minutes_to_departure', 'child_count', 'weather_complexity', 'missing_item_count', 'historical_delay_min'], RandomForestClassifier(n_estimators=180, random_state=42))
    elif args.task == 'forgot':
        df = load_dataset(args.data, forgot_dataset)
        train_classifier(df, ['minutes_to_departure', 'bag_lifted', 'lunch_packed', 'schedule_change', 'interventions'], RandomForestClassifier(n_estimators=140, random_state=42))
    elif args.task == 'lunch':
        df = load_dataset(args.data, lunch_dataset)
        train_classifier(df, ['mass_grams', 'ice_pack_present', 'plate_temp_c', 'ambient_temp_c', 'minutes_until_lunch'], GradientBoostingClassifier(random_state=42))
    elif args.task == 'route':
        df = load_dataset(args.data, route_dataset)
        model = IsolationForest(random_state=42, contamination=0.12)
        model.fit(df[['route_minutes', 'expected_minutes', 'boarded', 'child_present', 'backpack_present']])
        scores = model.decision_function(df[['route_minutes', 'expected_minutes', 'boarded', 'child_present', 'backpack_present']])
        print({'score_mean': float(scores.mean()), 'score_min': float(scores.min()), 'rows': len(df)})
    else:
        raise SystemExit(f'unknown task: {args.task}')


if __name__ == '__main__':
    main()
