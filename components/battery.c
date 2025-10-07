/* See LICENSE file for copyright and license details. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>


#include "../slstatus.h"
#include "../util.h"

#if defined(__linux__)
/*
 * https://www.kernel.org/doc/html/latest/power/power_supply_class.html
 */
	#include <limits.h>
	#include <stdint.h>
	#include <unistd.h>

	#define POWER_SUPPLY_CAPACITY "/sys/class/power_supply/%s/capacity"
	#define POWER_SUPPLY_STATUS   "/sys/class/power_supply/%s/status"
	#define POWER_SUPPLY_CHARGE   "/sys/class/power_supply/%s/charge_now"
	#define POWER_SUPPLY_ENERGY   "/sys/class/power_supply/%s/energy_now"
	#define POWER_SUPPLY_CURRENT  "/sys/class/power_supply/%s/current_now"
	#define POWER_SUPPLY_POWER    "/sys/class/power_supply/%s/power_now"
	#define POWER_SUPPLY_VOLTAGE  "/sys/class/power_supply/%s/voltage_now"
	#define POWER_SUPPLY_CHARGE_FULL "/sys/class/power_supply/%s/charge_full"
	#define POWER_SUPPLY_CHARGE_FULL_DESIGN "/sys/class/power_supply/%s/charge_full_design"

	static const char *
	pick(const char *bat, const char *f1, const char *f2, char *path,
	     size_t length)
	{
		if (esnprintf(path, length, f1, bat) > 0 &&
		    access(path, R_OK) == 0)
			return f1;

		if (esnprintf(path, length, f2, bat) > 0 &&
		    access(path, R_OK) == 0)
			return f2;

		return NULL;
	}

	const char *
	battery_perc(const char *bat)
	{
		int cap_perc;
		char path[PATH_MAX];

		if (esnprintf(path, sizeof(path), POWER_SUPPLY_CAPACITY, bat) < 0)
			return NULL;
		if (pscanf(path, "%d", &cap_perc) != 1)
			return NULL;

		return bprintf("%d", cap_perc);
	}

	const char *
	battery_state(const char *bat)
	{
		static struct {
			char *state;
			char *symbol;
		} map[] = {
			{ "Charging",    "+" },
			{ "Discharging", "-" },
			{ "Full",        "o" },
			{ "Not charging", "o" },
		};
		size_t i;
		char path[PATH_MAX], state[12];

		if (esnprintf(path, sizeof(path), POWER_SUPPLY_STATUS, bat) < 0)
			return NULL;
		if (pscanf(path, "%12[a-zA-Z ]", state) != 1)
			return NULL;

		for (i = 0; i < LEN(map); i++)
			if (!strcmp(map[i].state, state))
				break;

		return (i == LEN(map)) ? "?" : map[i].symbol;
	}



	const char *
	battery_remaining(const char *bat)
	{
		char path[PATH_MAX], state[16];
		double charge_now, current_now, timeleft;
		int h, m;

		// Read battery status (Charging/Discharging)
		if (esnprintf(path, sizeof(path), POWER_SUPPLY_STATUS, bat) < 0)
			return NULL;
		if (pscanf(path, "%15s", state) != 1)
			return NULL;

		// Read charge_now or energy_now
		if (!pick(bat, POWER_SUPPLY_CHARGE, POWER_SUPPLY_ENERGY, path, sizeof(path)))
			return NULL;
		if (pscanf(path, "%lf", &charge_now) != 1)
			return NULL;

		// Only calculate time if discharging or charging
		if (!strcmp(state, "Discharging") || !strcmp(state, "Charging")) {
			if (!pick(bat, POWER_SUPPLY_CURRENT, POWER_SUPPLY_POWER, path, sizeof(path)))
				return NULL;

			FILE *f = fopen(path, "r");
			if (!f)
				return NULL;

			char buf[64];
			if (!fgets(buf, sizeof(buf), f)) {
				fclose(f);
				return NULL;
			}
			fclose(f);

			current_now = fabs(atof(buf));
			if (current_now <= 0.0)
				return NULL;

			timeleft = charge_now / current_now;
			h = (int)timeleft;
			m = (int)((timeleft - h) * 60);

			if (!strcmp(state, "Charging"))
				return bprintf("%dh %dm to full", h, m);
			else
				return bprintf("%dh %dm left", h, m);
		}

		return "";
	}

	const char *
	battery_wattage(const char *bat)
	{
		char path[PATH_MAX], state[16];
		double current_now, voltage_now, watt;

		// Read battery state (Charging/Discharging)
		if (esnprintf(path, sizeof(path), POWER_SUPPLY_STATUS, bat) < 0)
			return NULL;
		if (pscanf(path, "%15s", state) != 1)
			return NULL;

		// Read current_now (µA)
		if (esnprintf(path, sizeof(path), POWER_SUPPLY_CURRENT, bat) < 0)
			return NULL;
		if (pscanf(path, "%lf", &current_now) != 1)
			return NULL;

		// Read voltage_now (µV)
		if (esnprintf(path, sizeof(path), POWER_SUPPLY_VOLTAGE, bat) < 0)
			return NULL;
		if (pscanf(path, "%lf", &voltage_now) != 1)
			return NULL;

		// Compute W = µA * µV / 1e12
		watt = (current_now * voltage_now) / 1000000000000.0;

		if (strcmp(state, "Charging") == 0)
			return bprintf("+%.2fW", watt);
		else
			return bprintf("%.2fW", watt);
	}

	const char *
	battery_health(const char *bat)
	{
		char path[PATH_MAX];
		uintmax_t full, design;

		if (esnprintf(path, sizeof(path), POWER_SUPPLY_CHARGE_FULL, bat) < 0 ||
		    pscanf(path, "%ju", &full) < 0)
			return NULL;

		if (esnprintf(path, sizeof(path), POWER_SUPPLY_CHARGE_FULL_DESIGN, bat) < 0 ||
		    pscanf(path, "%ju", &design) < 0)
			return NULL;

		if (design == 0)
			return NULL;

		return bprintf("%ju%%", full * 100 / design);
	}

#elif defined(__OpenBSD__)
/* unchanged */
#elif defined(__FreeBSD__)
/* unchanged */
#endif

