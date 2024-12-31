/*
 *  Copyright (c) 1999-2000 Vojtech Pavlik
 *  Copyright (c) 2009-2011 Red Hat, Inc
 *  Copyright (c) 2024 Jovie LaRue
 */

/**
 * @file
 * Event device test program
 *
 * kbstats prints the capabilities on the kernel devices in /dev/input/eventX
 * and their events. Its primary purpose is for kernel or X driver
 * debugging.
 *
 * Manually compile with
 * gcc -o kbstats kbstats.c
 */

/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * kb-stats is a modified version of the original program, evtest, by Vojtech
 * Pavlik. It will behave differently than evtest.
 */

#define _GNU_SOURCE /* for asprintf */
#include <stdint.h>
#include <stdio.h>
#include <time.h>

#if HAVE_CONFIG_H
#include <config.h>
#endif

#include <linux/input.h>
#include <linux/version.h>

#include <ctype.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

#define BITS_PER_LONG (sizeof(long) * 8)
#define NBITS(x) ((((x) - 1) / BITS_PER_LONG) + 1)
#define OFF(x) ((x) % BITS_PER_LONG)
#define BIT(x) (1UL << OFF(x))
#define LONG(x) ((x) / BITS_PER_LONG)
#define test_bit(bit, array) ((array[LONG(bit)] >> OFF(bit)) & 1)

#define DEV_INPUT_EVENT "/dev/input"
#define EVENT_DEV_NAME "event"

#ifndef EV_SYN
#define EV_SYN 0
#endif
#ifndef SYN_MAX
#define SYN_MAX 3
#define SYN_CNT (SYN_MAX + 1)
#endif
#ifndef SYN_MT_REPORT
#define SYN_MT_REPORT 2
#endif
#ifndef SYN_DROPPED
#define SYN_DROPPED 3
#endif

#define NAME_ELEMENT(element) [element] = #element

enum evtest_mode {
  MODE_CAPTURE,
  MODE_QUERY,
  MODE_VERSION,
};

static const struct query_mode {
  const char *name;
  int event_type;
  int max;
  int rq;
} query_modes[] = {
    {"EV_KEY", EV_KEY, KEY_MAX, EVIOCGKEY(KEY_MAX)},
};

static int grab_flag = 0;
static volatile sig_atomic_t stop = 0;

static void interrupt_handler(int sig) { stop = 1; }

/**
 * Look up an entry in the query_modes table by its textual name.
 *
 * @param mode The name of the entry to be found.
 *
 * @return The requested query_mode, or NULL if it could not be found.
 */
static const struct query_mode *find_query_mode_by_name(const char *name) {
  int i;
  for (i = 0; i < sizeof(query_modes) / sizeof(*query_modes); i++) {
    const struct query_mode *mode = &query_modes[i];
    if (strcmp(mode->name, name) == 0)
      return mode;
  }
  return NULL;
}

static const char *const events[EV_MAX + 1] = {
    [0 ... EV_MAX] = NULL,
    NAME_ELEMENT(EV_KEY),
};

static const int maxval[EV_MAX + 1] = {
    [0 ... EV_MAX] = -1,
    [EV_KEY] = KEY_MAX,
};

static const char *const keys[KEY_MAX + 1] = {
    [0 ... KEY_MAX] = NULL,
    NAME_ELEMENT(KEY_RESERVED),
    NAME_ELEMENT(KEY_ESC),
    NAME_ELEMENT(KEY_1),
    NAME_ELEMENT(KEY_2),
    NAME_ELEMENT(KEY_3),
    NAME_ELEMENT(KEY_4),
    NAME_ELEMENT(KEY_5),
    NAME_ELEMENT(KEY_6),
    NAME_ELEMENT(KEY_7),
    NAME_ELEMENT(KEY_8),
    NAME_ELEMENT(KEY_9),
    NAME_ELEMENT(KEY_0),
    NAME_ELEMENT(KEY_MINUS),
    NAME_ELEMENT(KEY_EQUAL),
    NAME_ELEMENT(KEY_BACKSPACE),
    NAME_ELEMENT(KEY_TAB),
    NAME_ELEMENT(KEY_Q),
    NAME_ELEMENT(KEY_W),
    NAME_ELEMENT(KEY_E),
    NAME_ELEMENT(KEY_R),
    NAME_ELEMENT(KEY_T),
    NAME_ELEMENT(KEY_Y),
    NAME_ELEMENT(KEY_U),
    NAME_ELEMENT(KEY_I),
    NAME_ELEMENT(KEY_O),
    NAME_ELEMENT(KEY_P),
    NAME_ELEMENT(KEY_LEFTBRACE),
    NAME_ELEMENT(KEY_RIGHTBRACE),
    NAME_ELEMENT(KEY_ENTER),
    NAME_ELEMENT(KEY_LEFTCTRL),
    NAME_ELEMENT(KEY_A),
    NAME_ELEMENT(KEY_S),
    NAME_ELEMENT(KEY_D),
    NAME_ELEMENT(KEY_F),
    NAME_ELEMENT(KEY_G),
    NAME_ELEMENT(KEY_H),
    NAME_ELEMENT(KEY_J),
    NAME_ELEMENT(KEY_K),
    NAME_ELEMENT(KEY_L),
    NAME_ELEMENT(KEY_SEMICOLON),
    NAME_ELEMENT(KEY_APOSTROPHE),
    NAME_ELEMENT(KEY_LEFTSHIFT),
    NAME_ELEMENT(KEY_BACKSLASH),
    NAME_ELEMENT(KEY_Z),
    NAME_ELEMENT(KEY_X),
    NAME_ELEMENT(KEY_C),
    NAME_ELEMENT(KEY_V),
    NAME_ELEMENT(KEY_B),
    NAME_ELEMENT(KEY_N),
    NAME_ELEMENT(KEY_M),
    NAME_ELEMENT(KEY_COMMA),
    NAME_ELEMENT(KEY_DOT),
    NAME_ELEMENT(KEY_SLASH),
    NAME_ELEMENT(KEY_RIGHTSHIFT),
    NAME_ELEMENT(KEY_KPASTERISK),
    NAME_ELEMENT(KEY_LEFTALT),
    NAME_ELEMENT(KEY_SPACE),
    NAME_ELEMENT(KEY_RIGHTCTRL),
    NAME_ELEMENT(KEY_RIGHTALT),
    NAME_ELEMENT(KEY_UP),
    NAME_ELEMENT(KEY_LEFT),
    NAME_ELEMENT(KEY_RIGHT),
    NAME_ELEMENT(KEY_END),
    NAME_ELEMENT(KEY_DOWN),
    NAME_ELEMENT(KEY_INSERT),
    NAME_ELEMENT(KEY_DELETE),
    NAME_ELEMENT(KEY_KPLEFTPAREN),
    NAME_ELEMENT(KEY_KPRIGHTPAREN),
#ifdef KEY_NUMERIC_0
    NAME_ELEMENT(KEY_NUMERIC_0),
    NAME_ELEMENT(KEY_NUMERIC_1),
    NAME_ELEMENT(KEY_NUMERIC_2),
    NAME_ELEMENT(KEY_NUMERIC_3),
    NAME_ELEMENT(KEY_NUMERIC_4),
    NAME_ELEMENT(KEY_NUMERIC_5),
    NAME_ELEMENT(KEY_NUMERIC_6),
    NAME_ELEMENT(KEY_NUMERIC_7),
    NAME_ELEMENT(KEY_NUMERIC_8),
    NAME_ELEMENT(KEY_NUMERIC_9),
    NAME_ELEMENT(KEY_NUMERIC_STAR),
    NAME_ELEMENT(KEY_NUMERIC_POUND),
#endif
};

static const char *const *const names[EV_MAX + 1] = {
    [0 ... EV_MAX] = NULL,
    [EV_KEY] = keys,
};

/**
 * Filter for the AutoDevProbe scandir on /dev/input.
 *
 * @param dir The current directory entry provided by scandir.
 *
 * @return Non-zero if the given directory entry starts with "event", or zero
 * otherwise.
 */
static int is_event_device(const struct dirent *dir) {
  return strncmp(EVENT_DEV_NAME, dir->d_name, 5) == 0;
}

/**
 * Scans all /dev/input/event*, display them and ask the user which one to
 * open.
 *
 * @return The event device file name of the device file selected. This
 * string is allocated and must be freed by the caller.
 */
static char *scan_devices(void) {
  struct dirent **namelist;
  int i, ndev, devnum;
  char *filename;
  int max_device = 0;

  ndev = scandir(DEV_INPUT_EVENT, &namelist, is_event_device, versionsort);
  if (ndev <= 0)
    return NULL;

  fprintf(stderr, "Available devices:\n");

  for (i = 0; i < ndev; i++) {
    char fname[64];
    int fd = -1;
    char name[256] = "???";

    snprintf(fname, sizeof(fname), "%s/%s", DEV_INPUT_EVENT,
             namelist[i]->d_name);
    fd = open(fname, O_RDONLY);
    if (fd < 0)
      continue;
    ioctl(fd, EVIOCGNAME(sizeof(name)), name);

    fprintf(stderr, "%s:	%s\n", fname, name);
    close(fd);

    sscanf(namelist[i]->d_name, "event%d", &devnum);
    if (devnum > max_device)
      max_device = devnum;

    free(namelist[i]);
  }

  fprintf(stderr, "Select the device event number [0-%d]: ", max_device);
  scanf("%d", &devnum);

  if (devnum > max_device || devnum < 0)
    return NULL;

  asprintf(&filename, "%s/%s%d", DEV_INPUT_EVENT, EVENT_DEV_NAME, devnum);

  return filename;
}

static int version(void) {
#ifndef PACKAGE_VERSION
#define PACKAGE_VERSION "<version undefined>"
#endif
  printf("%s %s\n", program_invocation_short_name, PACKAGE_VERSION);
  return EXIT_SUCCESS;
}

/**
 * Print usage information.
 */
static int usage(void) {
  printf("USAGE:\n");
  printf(" Capture mode:\n");
  printf("   %s [--grab] /dev/input/eventX\n", program_invocation_short_name);
  printf("     --grab  grab the device for exclusive access\n");
  printf("\n");
  printf(" Query mode: (check exit code)\n");
  printf("   %s --query /dev/input/eventX <type> <value>\n",
         program_invocation_short_name);

  printf("\n");
  printf("<type> should be EV_KEY\n");
  printf(
      "<value> can either be a numerical value, or the textual name of the\n");
  printf("key being queried (e.g. KEY_5).\n");

  return EXIT_FAILURE;
}

static inline const char *typename(unsigned int type) {
  return (type <= EV_MAX && events[type]) ? events[type] : "?";
}

static inline const char *codename(unsigned int type, unsigned int code) {
  return (type <= EV_MAX && code <= maxval[type] && names[type] &&
          names[type][code])
             ? names[type][code]
             : "?";
}

/**
 * Print static device information (no events). This information includes
 * version numbers, device name and all bits supported by this device.
 *
 * @param fd The file descriptor to the device.
 * @return 0 on success or 1 otherwise.
 */
static int print_device_info(int fd) {
  unsigned int type, code;
  int version;
  unsigned short id[4];
  char name[256] = "Unknown";
  unsigned long bit[EV_MAX][NBITS(KEY_MAX)];
#ifdef INPUT_PROP_SEMI_MT
  unsigned int prop;
  unsigned long propbits[INPUT_PROP_MAX];
#endif

  if (ioctl(fd, EVIOCGVERSION, &version)) {
    perror("evtest: can't get version");
    return 1;
  }

  printf("Input driver version is %d.%d.%d\n", version >> 16,
         (version >> 8) & 0xff, version & 0xff);

  ioctl(fd, EVIOCGID, id);
  printf("Input device ID: bus 0x%x vendor 0x%x product 0x%x version 0x%x\n",
         id[ID_BUS], id[ID_VENDOR], id[ID_PRODUCT], id[ID_VERSION]);

  ioctl(fd, EVIOCGNAME(sizeof(name)), name);
  printf("Input device name: \"%s\"\n", name);

  memset(bit, 0, sizeof(bit));
  ioctl(fd, EVIOCGBIT(0, EV_MAX), bit[0]);
  printf("Supported events:\n");

  return 0;
}

/**
 * Print device events as they come in.
 *
 * @param fd The file descriptor to the device.
 * @return 0 on success or 1 otherwise.
 */
static int print_events(int fd) {
  struct input_event event[64];
  int i, rd;
  fd_set rdfs;

  FD_ZERO(&rdfs);
  FD_SET(fd, &rdfs);

  const char *last_code_name = "";

  while (!stop) {
    select(fd + 1, &rdfs, NULL, NULL, NULL);
    if (stop)
      break;
    rd = read(fd, event, sizeof(event));

    if (rd < (int)sizeof(struct input_event)) {
      printf("expected %d bytes, got %d\n", (int)sizeof(struct input_event),
             rd);
      perror("\nevtest: error reading");
      return 1;
    }

    for (i = 0; i < rd / sizeof(struct input_event); i++) {
      unsigned int type, code;

      type = event[i].type;
      code = event[i].code;
      const char *code_name = codename(type, code);

      // create malloced code_name
      // and ptr to code_name_dup (to_free) to use in strtok
      char *to_free;
      char *code_name_dup = strdup(code_name);
      to_free = code_name_dup;

      // if code_name !contains "?" and last_code_name != code_name
      if (strstr(code_name, "?") == NULL && strcmp(last_code_name, code_name)) {
        char *prefix = strtok(code_name_dup, "_");
        char *delimited_name = strtok(NULL, "_");
        printf("%s\n", delimited_name);
        last_code_name = code_name;
      }

      free(code_name_dup);
    }
  }

  ioctl(fd, EVIOCGRAB, (void *)0);
  return EXIT_SUCCESS;
}

/**
 * Grab and immediately ungrab the device.
 *
 * @param fd The file descriptor to the device.
 * @return 0 if the grab was successful, or 1 otherwise.
 */
static int test_grab(int fd, int grab_flag) {
  int rc;

  rc = ioctl(fd, EVIOCGRAB, (void *)1);

  if (rc == 0 && !grab_flag)
    ioctl(fd, EVIOCGRAB, (void *)0);

  return rc;
}

/**
 * Enter capture mode. The requested event device will be monitored, and any
 * captured events will be decoded and printed on the console.
 *
 * @param device The device to monitor, or NULL if the user should be prompted.
 * @return 0 on success, non-zero on error.
 */
static int do_capture(const char *device, int grab_flag) {
  int fd;
  char *filename = NULL;

  if (!device) {
    fprintf(stderr, "No device specified, trying to scan all of %s/%s*\n",
            DEV_INPUT_EVENT, EVENT_DEV_NAME);

    if (getuid() != 0)
      fprintf(stderr, "Not running as root, no devices may be available.\n");

    filename = scan_devices();
    if (!filename)
      return usage();
  } else {
    filename = strdup(device);
  }

  if (!filename)
    return EXIT_FAILURE;

  if ((fd = open(filename, O_RDONLY)) < 0) {
    perror("evtest");
    if (errno == EACCES && getuid() != 0)
      fprintf(stderr,
              "You do not have access to %s. Try "
              "running as root instead.\n",
              filename);
    goto error;
  }

  if (!isatty(fileno(stdout)))
    setbuf(stdout, NULL);

  if (print_device_info(fd))
    goto error;

  printf("Testing ... (interrupt to exit)\n");

  if (test_grab(fd, grab_flag)) {
    printf("***********************************************\n");
    printf("  This device is grabbed by another process.\n");
    printf("  No events are available to evtest while the\n"
           "  other grab is active.\n");
    printf("  In most cases, this is caused by an X driver,\n"
           "  try VT-switching and re-run evtest again.\n");
    printf("  Run the following command to see processes with\n"
           "  an open fd on this device\n"
           " \"fuser -v %s\"\n",
           filename);
    printf("***********************************************\n");
  }

  signal(SIGINT, interrupt_handler);
  signal(SIGTERM, interrupt_handler);

  free(filename);

  return print_events(fd);

error:
  free(filename);
  return EXIT_FAILURE;
}

/**
 * Perform a one-shot state query on a specific device. The query can be of
 * any known mode, on any valid keycode.
 *
 * @param device Path to the evdev device node that should be queried.
 * @param query_mode The event type that is being queried (should be EV_KEY)
 * @param keycode The code of the key to be queried
 * @return 0 if the state bit is unset, 10 if the state bit is set, 1 on error.
 */
static int query_device(const char *device, const struct query_mode *query_mode,
                        int keycode) {
  int fd;
  int r;
  unsigned long state[NBITS(query_mode->max)];

  fd = open(device, O_RDONLY);
  if (fd < 0) {
    perror("open");
    return EXIT_FAILURE;
  }
  memset(state, 0, sizeof(state));
  r = ioctl(fd, query_mode->rq, state);
  close(fd);

  if (r == -1) {
    perror("ioctl");
    return EXIT_FAILURE;
  }

  if (test_bit(keycode, state))
    return 10; /* different from EXIT_FAILURE */
  else
    return 0;
}

int main(int argc, char **argv) {
  const char *device = NULL;
  const char *keyname;
  const char *event_type;
  enum evtest_mode mode = MODE_CAPTURE;

  if (mode == MODE_CAPTURE)
    return do_capture(device, grab_flag);

  if ((argc - optind) < 2) {
    fprintf(stderr, "Query mode requires device, type and key parameters\n");
    return usage();
  }
}
