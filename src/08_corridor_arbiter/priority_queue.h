#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H

void pq_init(void);
int pq_update_and_get_top(const char* vehicle_id, double eta_s, double ahead_m, int target_arm);

#endif
