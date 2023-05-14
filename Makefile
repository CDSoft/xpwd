PREFIX := $(firstword $(wildcard $(PREFIX) $(HOME)/.local $(HOME)))
BUILD = .build

CFLAGS = -Wall -Wextra -Werror -O3
LDFLAGS = -lX11

all: $(BUILD)/xpwd

install: $(PREFIX)/bin/xpwd

$(PREFIX)/bin/xpwd: $(BUILD)/xpwd
	install $< $@

$(BUILD)/xpwd: xpwd.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) $(LDFLAGS) $< -o $@
