/* Copyright (C) 2017 Adrien Guatto.
 *
 * MLton is released under a HPND-style license.
 * See the file MLton-LICENSE for details.
 */

#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#include "trace.h"

static const char *EventKindStrings[] = {
  [EVENT_NIL]                   = "NIL",
  [EVENT_INIT]                  = "INIT",
  [EVENT_LAUNCH]                = "LAUNCH",
  [EVENT_FINISH]                = "FINISH",

  [EVENT_GC_ENTER]              = "GC_ENTER",
  [EVENT_GC_LEAVE]              = "GC_LEAVE",
  [EVENT_GC_ABORT]              = "GC_ABORT",

  [EVENT_RUNTIME_ENTER]         = "RUNTIME_ENTER",
  [EVENT_RUNTIME_LEAVE]         = "RUNTIME_LEAVE",

  [EVENT_HALT_REQ]              = "HALT_REQ",
  [EVENT_HALT_WAIT]             = "HALT_WAIT",
  [EVENT_HALT_ACK]              = "HALT_ACK",

  [EVENT_THREAD_COPY]           = "THREAD_COPY",

  [EVENT_HEAP_OCCUPANCY]        = "HEAP_OCCUPANCY",

  [EVENT_CHUNKP_OCCUPANCY]      = "CHUNKP_OCCUPANCY",
  [EVENT_CHUNKP_RATIO]          = "CHUNKP_RATIO",

  [EVENT_LOCK_TAKE_ENTER]       = "LOCK_TAKE_ENTER",
  [EVENT_LOCK_TAKE_LEAVE]       = "LOCK_TAKE_LEAVE",

  [EVENT_RWLOCK_R_TAKE]         = "RWLOCK_R_TAKE",
  [EVENT_RWLOCK_R_RELEASE]      = "RWLOCK_R_RELEASE",
  [EVENT_RWLOCK_W_TAKE]         = "RWLOCK_W_TAKE",
  [EVENT_RWLOCK_W_RELEASE]      = "RWLOCK_W_RELEASE",

  [EVENT_GSECTION_BEGIN_ENTER]  = "GSECTION_BEGIN_ENTER",
  [EVENT_GSECTION_BEGIN_LEAVE]  = "GSECTION_BEGIN_LEAVE",
  [EVENT_GSECTION_END_ENTER]    = "GSECTION_END_ENTER",
  [EVENT_GSECTION_END_LEAVE]    = "GSECTION_END_LEAVE",

  [EVENT_ARRAY_ALLOCATE_ENTER]  = "ARRAY_ALLOCATE_ENTER",
  [EVENT_ARRAY_ALLOCATE_LEAVE]  = "ARRAY_ALLOCATE_LEAVE",

  [EVENT_PROMOTION_ENTER]       = "PROMOTION_ENTER",
  [EVENT_PROMOTION_LEAVE]       = "PROMOTION_LEAVE",
  [EVENT_PROMOTED_WRITE]        = "PROMOTED_WRITE",
  [EVENT_PROMOTION]             = "PROMOTION",

  [EVENT_MERGED_HEAP]           = "MERGED_HEAP",

  [EVENT_COPY]                  = "COPY",
};

void processFiles(size_t filecount, FILE **files, void (*func)(struct Event *));
void processFilesChromeTracingJSON(size_t filecount, FILE **files);

void printEventText(struct Event *);
void printEventCSV(struct Event *);
void printEventChromeTracingJSON(struct Event *);

void usage() {
  fprintf(stderr,
          "usage: trace [options] [input files]\n"
          "options:\n"
          "  -d                 display contents in human-readable format\n"
          "  -c                 display contents in CSV format\n"
          "  -j                 display contents in Chrome Tracing JSON format\n"
          "  -h                 display this message\n"
    );
}

int main(int argc, char *argv[]) {
  int opt;
  size_t fcount;
  bool display = false, csv = false, chromeTracingJSON = false;
  bool read_stdin = false;
  FILE **files;

  /* Parse command line arguments. */

  while ((opt = getopt(argc, argv, "dhcj")) != -1) {
    switch (opt) {
    case 'd':
      display = true;
      break;
    case 'c':
      csv = true;
      break;
    case 'j':
      chromeTracingJSON = true;
      break;
    case 'h':
      usage();
      return 0;
    default:
      fprintf(stderr, "invalid option '%c' (%d)\n", opt, opt);
      usage();
      return 1;
    }
  }

  fcount = argc - optind;

  /* Open the files, if any. */

  if (fcount == 0) {
    read_stdin = true;
    fcount = 1;
  }

  if ((files = calloc(fcount, sizeof *files)) == NULL) {
    fprintf(stderr, "Could not allocate memory\n");
    return 1;
  }

  if (read_stdin)
    files[0] = stdin;
  else
    for (int i = 0; i < fcount; ++i) {
      const char *fn = argv[optind + i];

      if ((files[i] = fopen(fn, "rb")) == NULL) {
        fprintf(stderr, "%s: could not open file\n", fn);
        return 1;
      }
    }

  /* Perform the requested actions. */

  if (display)
    processFiles(fcount, files, printEventText);

  if (csv)
    processFiles(fcount, files, printEventCSV);

  if (chromeTracingJSON)
    processFilesChromeTracingJSON(fcount, files);

  /* Close and free files. */

  if (!read_stdin)
    for (int i = 0; i < fcount; i++)
      fclose(files[i]);

  free(files);

  return 0;
}

#define BUFFER_SIZE 10000

void processFiles(size_t filecount, FILE **files,
                  void (*func)(struct Event *)) {
  struct Event events[BUFFER_SIZE];

  for (size_t i = 0; i < filecount; ++i) {
    size_t evcount = 0, evbatchsize;

    do {
      evbatchsize = fread(events, sizeof *events, BUFFER_SIZE,
                          files[i]);
      evcount += evbatchsize;

      for (int j = 0; j < evbatchsize; j++)
        func(&events[j]);

    } while (evbatchsize == BUFFER_SIZE);
  }
}

void processFilesChromeTracingJSON(size_t filecount, FILE **files)
{
  struct Event events[BUFFER_SIZE];

  printf("[\n");

  bool first = true;

  for (size_t i = 0; i < filecount; ++i) {
    size_t evcount = 0, evbatchsize;

    do {
      evbatchsize = fread(events, sizeof *events, BUFFER_SIZE,
                          files[i]);
      evcount += evbatchsize;

      for (int j = 0; j < evbatchsize; j++) {

        if (first) first = false;
        else printf(",\n");

        printf("  ");
        printEventChromeTracingJSON(&events[j]);
      }

    } while (evbatchsize == BUFFER_SIZE);
  }

  printf("]\n");
}

void printEventKind(int kind) {
  if (kind > 0 && (size_t)kind < EventKindCount) {
    printf("%s", EventKindStrings[kind]);
  } else {
    printf("USER(%d)", kind);
  }
}

/** print the name without _ENTER or _LEAVE at end */
void printEventKindStripEnterLeave(int kind) {
  char buf[100];
  buf[99] = '\0';

  const char *str = EventKindStrings[kind];
  size_t len = strlen(str);
  strncpy(buf, str, len-6);
  buf[len-6] = '\0';

  printf("%s", buf);
}

/** chrome tracing documented here:
  * https://docs.google.com/document/d/1CvAClvFfyA5R-PhYUmn5OOQtYMH4h6I0nSsKchNAySU
  *
  * if this document eventually disappears from the eternal web,
  * see chrome-tracing-docs.txt
  */
enum ChromeTracingPhaseType {
  CT_Begin,    // "B"
  CT_End,      // "E"
  CT_Instant   // "i"
};

enum ChromeTracingPhaseType
EventKindChromeTracingPhaseType(int kind)
{
  // default to "instant" event; most general.
  if (!(kind > 0 && (size_t)kind < EventKindCount))
    return CT_Instant;

  // check for "_ENTER" or "_LEAVE", which is the mltrace convention
  const char *str = EventKindStrings[kind];
  size_t len = strlen(str);

  if (len > 6 && 0 == strncmp(str+len-6, "_ENTER", 6))
    return CT_Begin;

  if (len > 6 && 0 == strncmp(str+len-6, "_LEAVE", 6))
    return CT_End;

  return CT_Instant;
}

void printEventTime(struct Event *event) {
  double time;

  time = event->ts.tv_sec + event->ts.tv_nsec / 1E9;
  printf("%.9f", time);
}

void printEventTimeMicroseconds(struct Event *event) {
  unsigned long long us;
  us = ((unsigned long long)event->ts.tv_sec) * 1000000ULL;
  us = us + ((unsigned long long)event->ts.tv_nsec) / 1000ULL;

  printf("%llu", us);
}

void printEventTimeNanoseconds(struct Event *event) {
  unsigned long long ns;
  ns = ((unsigned long long)event->ts.tv_sec) * 1000000000ULL;
  ns = ns + ((unsigned long long)event->ts.tv_nsec);

  printf("%llu", ns);
}

void printEventChromeTracingJSON(struct Event *event) {
  printf("{ ");

  /** name and ph */
  switch (EventKindChromeTracingPhaseType(event->kind)) {
    case CT_Begin:
      printf("\"name\": \"");
      printEventKindStripEnterLeave(event->kind);
      printf("\", ");
      printf("\"ph\": \"B\", ");
      break;

    case CT_End:
      printf("\"name\": \"");
      printEventKindStripEnterLeave(event->kind);
      printf("\", ");
      printf("\"ph\": \"E\", ");
      break;

    default:
      printf("\"name\": \""); printEventKind(event->kind); printf("\", ");
      printf("\"ph\": \"i\", ");
      break;
  }

  printf("\"cat\": \"RUNTIME\", ");
  printf("\"pid\": 0, ");
  printf("\"tid\": %"PRIdPTR", ", event->argptr);
  printf("\"ts\": "); printEventTimeMicroseconds(event); printf(", ");
  printf("\"args\": { \"1\": %lld, \"2\": %lld, \"3\": %lld }",
    event->arg1, event->arg2, event->arg3);

  printf("}");
}

void printEventCSV(struct Event *event) {
  printEventKind(event->kind);
  printf(",%" PRIdPTR ",", event->argptr);
  printEventTime(event);
  printf(",%lld,%lld,%lld\n", event->arg1, event->arg2, event->arg3);
}

void printEventText(struct Event *event) {
  printEventTime(event);
  printf(" ");
  printf("%" PRIuPTR, event->argptr);
  printf(" ");
  printEventKind(event->kind);
  printf("(");

  switch(event->kind) {
  case EVENT_INIT:
  case EVENT_LAUNCH:
  case EVENT_FINISH:
  case EVENT_GC_ENTER:
  case EVENT_GC_LEAVE:
  case EVENT_GC_ABORT:
  case EVENT_RUNTIME_ENTER:
  case EVENT_RUNTIME_LEAVE:
  case EVENT_HALT_REQ:
  case EVENT_HALT_WAIT:
  case EVENT_HALT_ACK:
  case EVENT_GSECTION_BEGIN_ENTER:
  case EVENT_GSECTION_BEGIN_LEAVE:
  case EVENT_GSECTION_END_ENTER:
  case EVENT_GSECTION_END_LEAVE:
  case EVENT_ARRAY_ALLOCATE_LEAVE:
    break;

  case EVENT_THREAD_COPY:
    printf("from = %llx, to = %llx", event->arg1, event->arg2);
    break;

  case EVENT_HEAP_OCCUPANCY:
    printf("size = %llx, allocated = %llx", event->arg1, event->arg2);
    break;

  case EVENT_CHUNKP_OCCUPANCY:
    printf("size = %llx, allocated = %llx", event->arg1, event->arg2);
    break;

  case EVENT_CHUNKP_RATIO:
    printf("LCHsize = %llx, LCsize = %llx, ratio = %lld",
           event->arg1, event->arg2, event->arg3);
    break;

  case EVENT_LOCK_TAKE_ENTER:
  case EVENT_LOCK_TAKE_LEAVE:
    printf("lock = %llx", event->arg1);
    break;

  case EVENT_RWLOCK_R_TAKE:
  case EVENT_RWLOCK_R_RELEASE:
    printf("lock = %llx, readers = %llu", event->arg1, event->arg2);
    break;

  case EVENT_RWLOCK_W_TAKE:
  case EVENT_RWLOCK_W_RELEASE:
    printf("lock = %llx", event->arg1);
    break;

  case EVENT_ARRAY_ALLOCATE_ENTER:
    printf("ensureBytesFree = %llx, numElements = %lld, header = %llx",
           event->arg1, event->arg2, event->arg3);
    break;

  case EVENT_PROMOTION_ENTER:
    printf("src = %llx, dest_chunk = %llx", event->arg1, event->arg2);
    break;

  case EVENT_PROMOTED_WRITE:
    printf("orig_o = %llx, o = %llx", event->arg1, event->arg2);
    break;

  case EVENT_PROMOTION:
    printf("src = %llx, repl = %llx", event->arg1, event->arg2);
    break;

  case EVENT_MERGED_HEAP:
    printf("parenthh = %llx, childhh = %llx", event->arg1, event->arg2);
    break;

  case EVENT_COPY:
    printf("bytes = %lld, obj = %lld, stack = %lld",
           event->arg1, event->arg2, event->arg3);
    break;

  default:
    printf("?1 = %llx, ?2 = %llx, ?3 = %llx",
           event->arg1, event->arg2, event->arg3);
  }

  printf(")\n");
}
