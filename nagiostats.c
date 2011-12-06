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

#include <collectd/collectd.h>
#include <collectd/common.h>
#include <collectd/plugin.h>

#define BUFFSIZE 4096
#define HOST_START "host {\n"
#define SERVICE_START "service {\n"
#define STATUS_LINE "current_state="

static char *retentionfile = NULL;

static const char *config_keys[] =
{
  "RetentionFile"
};

static int config_keys_num = STATIC_ARRAY_SIZE (config_keys);

static void submit_gauge (const char *type, const char *type_inst, gauge_t value) /* {{{ */
{
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
/* }}} */


/*
 * This function is called in regular intervalls to collect the data.
 */
static int nagiostats_read (void)
{
  FILE *fp;
  char line[BUFFSIZE];
  int mode=0;
  int hosts[2]={0};
  int services[4]={0};
  int state_value;

  fp=fopen(retentionfile,"r");
  if (fp == NULL)
    return 1;

  while(fgets(line, sizeof(line), fp ) != NULL) {
    if(strcmp(line, HOST_START) == 0)
      mode=1;
    if(strcmp(line, SERVICE_START) == 0)
      mode=2;
    if(strcmp(line, "}\n") == 0)
      mode=0;
    if(strlen(line) == 16)
      if(strncmp(line, STATUS_LINE, 14) == 0) {
        state_value=atoi(&line[14]);
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
} /* static int nagiostats_read (void) */

static int nagiostats_config (const char *key, const char *value) /* {{{ */
{
  if (strcasecmp (key, "RetentionFile") == 0) {
    if (retentionfile != NULL) {
      free (retentionfile);
    }
    retentionfile = strdup (value);
  }

  return 0;
}
/* }}} */

/*
 * This function is called after loading the plugin to register it with
 * collectd.
 */
void module_register (void)
{
  plugin_register_config("nagiostats", nagiostats_config, config_keys, config_keys_num);
  plugin_register_read("nagiostats", nagiostats_read);
  return;
} /* void module_register (void) */

