# ifndef __NAGIOS_STATS_H

#define __NAGIOS_STATS_H 1

struct nagios_stats {
  int counter;
  int within_1_min;
  int within_5_min;
  int within_15_min;
  int within_60_min;
  
  double exec_time_min;
  double exec_time_max;
  double exec_time_sum;
  double latency_time_min;
  double latency_time_max;
  double latency_time_sum;
};

typedef struct nagios_stats nagios_stats;

nagios_stats init_nagios_stats(void);

static int read_performance (void);
static int read_status (void);
void submit_nagios_stats(nagios_stats s, const char *label);

#endif
