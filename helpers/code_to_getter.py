print """
#include <pebble.h>
#include <stdint.h>

int32_t weather_code_to_resource_id(int32_t weather_code)
{
	time_t now_s = time(NULL);
	struct tm* now = localtime(&now_s);
	uint8_t is_night = (now->tm_hour < 7 || now->tm_hour > 19) ? 1 : 0;
	switch (weather_code) {
"""

with open("owm-code-guide.txt", "r") as f:
    for line in f:
        if not line:
            continue
        if line[0].isdigit():
            rest, day_symbol, night_symbol = line.rstrip().rsplit(None, 2)
            code = int(rest.split()[0])
            day_symbol = day_symbol.upper()
            night_symbol = night_symbol.upper()
            print """
		case {code}:
			return is_night ? RESOURCE_ID_{night_symbol} : RESOURCE_ID_{day_symbol};
			break;
""".format(code=code, day_symbol=day_symbol, night_symbol=night_symbol)

print """
		default:
			return -1;
	}
}
"""
