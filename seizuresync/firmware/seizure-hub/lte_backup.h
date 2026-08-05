/* SeizureSync — 4G LTE backup (SIM7600G) */
#ifndef LTE_BACKUP_H
#define LTE_BACKUP_H
void lte_backup_init(int tx, int rx, int pwrkey);
int  lte_send_alert(const char *message);
#endif