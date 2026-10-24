# Host unit tests for the portable firmware layers (no vendor SDK, no hardware).
#   make -C tests          build + run tests under ASan/UBSan
#   make -C tests syntax   compile-check app + device drivers against PAL headers only

CC      ?= gcc
ROOT    := ..
INC     := -I$(ROOT)/config -I$(ROOT)/lib -I$(ROOT)/pal/include -I$(ROOT)/drivers \
           -I$(ROOT)/drivers/w25q -I$(ROOT)/drivers/rtc -I$(ROOT)/drivers/gnss \
           -I$(ROOT)/drivers/touch -I$(ROOT)/drivers/psoc_link -I$(ROOT)/middleware/j1939 \
           -I$(ROOT)/services -I$(ROOT)/gui -I$(ROOT)/app -I.
CFLAGS  := -std=gnu11 -O1 -g -Wall -Wextra -Werror -Wno-unused-function -DPAL_HOST \
           -fsanitize=address,undefined -fno-omit-frame-pointer $(INC)

SRC := test_main.c osal_host.c \
       $(ROOT)/lib/crc.c \
       $(ROOT)/drivers/gnss/nmea.c \
       $(ROOT)/drivers/psoc_link/psoc_link.c \
       $(ROOT)/middleware/j1939/j1939.c \
       $(ROOT)/middleware/j1939/j1939_signals.c \
       $(ROOT)/services/vehicle_data.c \
       $(ROOT)/services/datalog.c \
       $(ROOT)/services/nvstore.c \
       $(ROOT)/services/fuel_runtime.c \
       $(ROOT)/services/gps_services.c \
       $(ROOT)/services/anomaly.c \
       $(ROOT)/services/fleet_ble.c \
       $(ROOT)/gui/ui_model.c

SYNTAX_SRC := $(ROOT)/app/app_tasks.c $(ROOT)/app/main.c \
              $(ROOT)/drivers/w25q/w25q.c $(ROOT)/drivers/rtc/rv3028.c $(ROOT)/drivers/touch/gt911.c

all: run

test_runner: $(SRC) $(wildcard *.h) $(wildcard $(ROOT)/*/*.h $(ROOT)/*/*/*.h)
	$(CC) $(CFLAGS) $(SRC) -o $@ -lm

run: test_runner
	./test_runner

syntax:
	@for f in $(SYNTAX_SRC); do echo "  CC -fsyntax-only $$f"; \
	  $(CC) -std=gnu11 -Wall -Wextra -Werror -Wno-unused-parameter -fsyntax-only $(INC) $$f || exit 1; done

clean:
	rm -f test_runner

.PHONY: all run syntax clean
