/*
 * A nagios stats plugin for collectd.
 * Written by Nicolas Szalay <nico@rottenbytes.info>
 */

#include <stdio.h>

#if ! HAVE_CONFIG_H

#include <stdlib.h>
#include <string.h>

#ifndef __USE_ISOC99 /* required for NAN */
# define DISABLE_ISOC99 1
# define __USE_ISOC99 1
#endif /* !defined(__USE_ISOC99) */
#include <math.h>
#if DISABLE_ISOC99
# undef DISABLE_ISOC99
# undef __USE_ISOC99
#endif /* DISABLE_ISOC99 */

#include <time.h>

#endif /* ! HAVE_CONFIG */

#include <collectd.h>
#include <common.h>
#include <plugin.h>

#include "nagiostats.h"

#define BUFFSIZE 4096
#define STATUS_LINE "\tcurrent_state="

#define PERF_HOST_START "hoststatus {\n"
#define PERF_SERVICE_START "servicestatus {\n"
#define PERF_LAST_CHECK "\tlast_check="
#define PERF_EXEC_TIME "\tcheck_execution_time="
#define PERF_LATENCY "\tcheck_latency="
#define PERF_HOSTNAME "\thost_name="

#define HOST_CHECKS_ENABLED "\tactive_host_checks_enabled="
#define SERVICE_CHECKS_ENABLED "\tactive_service_checks_enabled="

static char *statusfile = NULL;

bool service_checks_enabled = false;
bool host_checks_enabled = false;

static const char *config_keys[] =
{
  "StatusFile"
};

static int config_keys_num = STATIC_ARRAY_SIZE (config_keys);

/* init data structure */
nagios_stats init_nagios_stats(void) {
  nagios_stats s;

  s.counter = 0;
  s.within_1_min = 0;
  s.within_5_min = 0;
  s.within_15_min = 0;
  s.within_60_min = 0;

  s.exec_time_min = 1000.0;
  s.exec_time_max = 0.0;
  s.exec_time_sum = 0.0;

  s.latency_time_min = 1000.0;
  s.latency_time_max = 0.0;
  s.latency_time_sum = 0.0;

  return s;
}

static void submit_gauge (const char *type, const char *type_inst, gauge_t value) {
  value_t values[1];
  value_list_t vl = VALUE_LIST_INIT;

  values[0].gauge = value;

  vl.values = values;
  vl.values_len = 1;
  sstrncpy (vl.host, hostname_g, sizeof (vl.host));
  sstrncpy (vl.plugin, "nagiostats", sizeof (vl.plugin));
  sstrncpy (vl.type, type, sizeof (vl.type));
  if (type_inst != NULL)
    sstrncpy (vl.type_instance, type_inst, sizeof (vl.type_instance));

  plugin_dispatch_values (&vl);
}

/* submit data structure, and do the calculations too */
void submit_nagios_stats(nagios_stats s, const char *label) {
  value_t range_values[4];
  value_list_t range_vl = VALUE_LIST_INIT;

  value_t exec_values[3];
  value_list_t exec_vl = VALUE_LIST_INIT;

  value_t latency_values[3];
  value_list_t latency_vl = VALUE_LIST_INIT;

  char tmp_label[BUFFSIZE];

  /* Time ranges : group all together */
#ifdef __DEBUG__  
  WARNING("=== %s ===", label);
  WARNING("within 1 min : %i / %i => %lf", s.within_1_min, s.counter, ((double)s.within_1_min / s.counter) * 100.0);
  WARNING("within 5 min : %i / %i => %lf", s.within_5_min, s.counter, ((double)s.within_5_min / s.counter) * 100.0);
  WARNING("within 15 min : %i / %i => %lf", s.within_15_min, s.counter, ((double)s.within_15_min / s.counter) * 100.0);
  WARNING("within 60 min : %i / %i => %lf", s.within_60_min, s.counter, ((double)s.within_60_min / s.counter) * 100.0);
#endif

  range_values[0].gauge = ((double)s.within_1_min / s.counter) * 100.0;
  range_values[1].gauge = ((double)s.within_5_min / s.counter) * 100.0;
  range_values[2].gauge =  ((double)s.within_15_min / s.counter) * 100.0;
  range_values[3].gauge =  ((double)s.within_60_min / s.counter) * 100.0;

  range_vl.values = range_values;
  range_vl.values_len = 4;
  sstrncpy(range_vl.host, hostname_g, sizeof (range_vl.host));
  sstrncpy(range_vl.plugin, "nagiostats", sizeof (range_vl.plugin));
  // Custom type here, to group metrics and make it readable
  // nagios_range              1_minute:GAUGE:0:100, 5_minutes:GAUGE:0:100, 15_minute:GAUGE:0:100, 60_minutes:GAUGE:0:100
  sstrncpy(range_vl.type, "nagios_range", sizeof (range_vl.type));
  strcpy(tmp_label, "");
  strcat(tmp_label, label);
  strcat(tmp_label, "_duration_ranges");
  sstrncpy (range_vl.type_instance, tmp_label, sizeof (range_vl.type_instance));
  plugin_dispatch_values (&range_vl);

  /* execution time : group min, max and avg in the same graph */
#ifdef __DEBUG__
  WARNING("min exec time : %lf", s.exec_time_min);
  WARNING("max exec time : %lf", s.exec_time_max);
  WARNING("avg exec time : %lf", s.exec_time_sum / (s.counter * 1.0));
#endif

  exec_values[0].gauge = s.exec_time_min;
  exec_values[1].gauge = s.exec_time_max;
  exec_values[2].gauge = (s.exec_time_sum / (s.counter * 1.0));

  exec_vl.values = exec_values;
  exec_vl.values_len = 3;
  sstrncpy(exec_vl.host, hostname_g, sizeof (exec_vl.host));
  sstrncpy(exec_vl.plugin, "nagiostats", sizeof (exec_vl.plugin));
  // Same as above
  // nagios_time               min:GAUGE:0:U, max:GAUGE:0:U, average:GAUGE:0:U
  sstrncpy(exec_vl.type, "nagios_time", sizeof (exec_vl.type));
  strcpy(tmp_label, "");
  strcat(tmp_label, label);
  strcat(tmp_label, "_execution_time");
  sstrncpy (exec_vl.type_instance, tmp_label, sizeof (exec_vl.type_instance));
  plugin_dispatch_values (&exec_vl);


  /* same for latency */
#ifdef __DEBUG__
  WARNING("min latency time : %lf", s.latency_time_min);
  WARNING("max latency time : %lf", s.latency_time_max);
  WARNING("avg latency time : %lf", s.latency_time_sum / (s.counter * 1.0));
#endif

  latency_values[0].gauge = s.latency_time_min;
  latency_values[1].gauge = s.latency_time_max;
  latency_values[2].gauge = (s.latency_time_sum / (s.counter * 1.0));

  latency_vl.values = latency_values;
  latency_vl.values_len = 3;
  sstrncpy(latency_vl.host, hostname_g, sizeof (latency_vl.host));
  sstrncpy(latency_vl.plugin, "nagiostats", sizeof (latency_vl.plugin));
  sstrncpy(latency_vl.type, "nagios_time", sizeof (latency_vl.type));
  strcpy(tmp_label, "");
  strcat(tmp_label, label);
  strcat(tmp_label, "_execution_latency");
  sstrncpy (latency_vl.type_instance, tmp_label, sizeof (latency_vl.type_instance));
  plugin_dispatch_values (&latency_vl);
}

/*
 * This function is called in regular intervalls to collect the data.
 */
static int nagiostats_read (void) {
  if (statusfile != NULL) { 
    read_status();
    read_performance();
  }
  return 0;
} /* static int nagiostats_read (void) */

/* Read service status */
static int read_status (void) {
  FILE *fp;
  char line[BUFFSIZE];
  int mode=0;
  int hosts[2]={0};
  int services[4]={0};
  int state_value;

  fp=fopen(statusfile,"r");
  if (fp == NULL)
    return 1;

  while(fgets(line, sizeof(line), fp ) != NULL) {
    if(strcmp(line, PERF_HOST_START) == 0)
      mode=1;
    if(strcmp(line, PERF_SERVICE_START) == 0)
      mode=2;
    if(strcmp(line, "}\n") == 0)
      mode=0;
    if(strlen(line) == 17)
      if(strncmp(line, STATUS_LINE, 15) == 0) {
        state_value=atoi(&line[15]);
        switch(mode) {
          case 1:
            switch(state_value) {
              case 0:
              case 1: // up
                hosts[0]++;
                break;
              case 2:
              case 3: // down
                hosts[1]++;
                break;
            }
            break;
          case 2:
            services[state_value]++;
            break;
        }
    }
  }
  fclose(fp);

  /* submit ALL the values !!! */
  submit_gauge("gauge","hosts_up",hosts[0]);
  submit_gauge("gauge","hosts_down",hosts[1]);

  submit_gauge("gauge","services_ok",services[0]);
  submit_gauge("gauge","services_warning",services[1]);
  submit_gauge("gauge","services_critical",services[2]);
  submit_gauge("gauge","services_unknown",services[3]);

  return 0;
}

/* Read global nagios performance */
static int read_performance (void)  {
  FILE *fp;
  char line[BUFFSIZE], hostname[BUFFSIZE];
  int mode=0;
  int i_value;
  double d_value;
  int reference_time=0, now, ago;
  struct stat file_info;

  nagios_stats host_perfs = init_nagios_stats();
  nagios_stats service_perfs = init_nagios_stats();

  /* find the last_check highest timestamp */
  fp=fopen(statusfile,"r");
  if (fp == NULL)
    return 1;

  while(fgets(line, sizeof(line), fp ) != NULL) {
    if(strncmp(line, PERF_LAST_CHECK, 12) == 0) {
      if ((atoi(&line[12]) > reference_time)) {
        reference_time=(atoi(&line[12]));
      }
    }
  }
  fclose(fp);

#ifdef __DEBUG__
  WARNING("Using %d as a reference time", reference_time);
#endif

  if(stat(statusfile,&file_info) == 0) {
    now = time(NULL);
    // file has not been updated for more than one minute, it's way too much !
    if (now - file_info.st_mtime > 60) {
      WARNING("Status file mtime timestamp is %d, when system time is %d. Too much delta, failing.", reference_time, now);
    }
  }
  else
  {
    WARNING("could not get last modification date of status file");
    return 1;
  }

  fp=fopen(statusfile,"r");
  if (fp == NULL)
    return 1;

  while(fgets(line, sizeof(line), fp ) != NULL) {
    if(strcmp(line, PERF_HOST_START) == 0)
      mode=1;
    if(strcmp(line, PERF_SERVICE_START) == 0)
      mode=2;
    if(strcmp(line, "}\n") == 0)
      mode=0;

    if(strncmp(line, PERF_HOSTNAME, 11) == 0) {
      strcpy(hostname, "                                 ");
      strncpy(hostname, &line[11], strlen(&line[11])-2);
    }

    if (strncmp(line, HOST_CHECKS_ENABLED, 28) == 0) {
      if (atoi(&line[28]) == 1) {
        host_checks_enabled = true;
#ifdef __DEBUG__
        WARNING("host checks now submitting data");
#endif
     } else {
        host_checks_enabled = false;
#ifdef __DEBUG__
        WARNING("host checks no more submitting data");
#endif
     }
    }

    if (strncmp(line, SERVICE_CHECKS_ENABLED, 31) == 0) {
      if (atoi(&line[31]) == 1) {
        service_checks_enabled = true;
#ifdef __DEBUG__
        WARNING("service checks now submitting data");
#endif
      } else {
        service_checks_enabled = false;
#ifdef __DEBUG__
        WARNING("service checks no more submitting data");
#endif
      }
    }

    /* Find the performance lines

      get the last check timestamp
    */
    if(strncmp(line, PERF_LAST_CHECK, 12) == 0) {
      i_value=atoi(&line[12]);
      ago = reference_time - i_value;

      switch(mode) {
        // host
        case 1:
          host_perfs.counter++; // number of hosts counter
          if (ago < 60) { // 1 minute
            host_perfs.within_1_min++;
          }
          if (ago < 300 ) { // 5 minutes
            host_perfs.within_5_min++;
          }
          if (ago < 900 ) { // 15 minutes
            host_perfs.within_15_min++;
          }
         if (ago < 3600 ) {  // 1 hour
            host_perfs.within_60_min++;
          }
        break;
        // service
        case 2:
          service_perfs.counter++; // number of services counter
          if (ago < 60) { // 1 minute
            service_perfs.within_1_min++;
          }
          if (ago < 300 ) { // 5 minutes
            service_perfs.within_5_min++;
          }
          if (ago < 900 ) { // 15 minutes
            service_perfs.within_15_min++;
          }
         if (ago < 3600 ) {  // 1 hour
            service_perfs.within_60_min++;
          }

          break;
      }
    }

    /* check execution time */
    if(strncmp(line, PERF_EXEC_TIME, 22) == 0) {
      d_value=strtod(&line[22],NULL);
      switch(mode) {
        // host
        case 1:
          host_perfs.exec_time_sum += d_value;
          // we are the new minimum
          if (d_value < host_perfs.exec_time_min) {
            host_perfs.exec_time_min = d_value;
          }
          // or maximum ?
          if (d_value > host_perfs.exec_time_max) {
            host_perfs.exec_time_max = d_value;
          }
        break;
        //service
        case 2:
          service_perfs.exec_time_sum += d_value;
          // we are the new minimum
          if (d_value < service_perfs.exec_time_min) {
            service_perfs.exec_time_min = d_value;
          }
          // or maximum ?
          if (d_value > service_perfs.exec_time_max) {
            service_perfs.exec_time_max = d_value;
          }

        break;
      }
    }

    /* check latency time */
    if(strncmp(line, PERF_LATENCY, 15) == 0) {
      d_value=strtod(&line[15],NULL);
      switch(mode) {
        // host
        case 1:
          host_perfs.latency_time_sum += d_value;
          // we are the new minimum
          if (d_value < host_perfs.latency_time_min) {
            host_perfs.latency_time_min = d_value;
          }
          // or maximum ?
          if (d_value > host_perfs.latency_time_max) {
            host_perfs.latency_time_max = d_value;
          }
        break;
        //service
        case 2:
          service_perfs.latency_time_sum += d_value;
          // we are the new minimum
          if (d_value < service_perfs.latency_time_min) {
            service_perfs.latency_time_min = d_value;
          }
          // or maximum ?
          if (d_value > service_perfs.latency_time_max) {
            service_perfs.latency_time_max = d_value;
          }
        break;
      }
    }
  }

  fclose(fp);
  if (host_checks_enabled == true) {
    submit_nagios_stats(host_perfs, "hosts");
#ifdef __DEBUG__
    WARNING("pushing host perfs");
#endif
  }
  if (service_checks_enabled == true) {
    submit_nagios_stats(service_perfs, "services");
#ifdef __DEBUG__
    WARNING("pushing service perfs");
#endif
 }

  return 0;
}

static int nagiostats_config (const char *key, const char *value) {
  if (strcasecmp (key, "StatusFile") == 0) {
    if (statusfile != NULL) {
      free (statusfile);
    }
    statusfile = strdup (value);
  }

  return 0;
}

/*
 * This function is called after loading the plugin to register it with
 * collectd.
 */
void module_register (void) {
  plugin_register_config("nagiostats", nagiostats_config, config_keys, config_keys_num);
  plugin_register_read("nagiostats", nagiostats_read);
  return;
} /* void module_register (void) */

