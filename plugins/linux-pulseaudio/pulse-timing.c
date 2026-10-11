/*
This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <util/c99defs.h>
#include <util/util_uint64.h>

#include "pulse-timing.h"

#define NSEC_PER_SEC 1000000000ULL

uint64_t pulse_timing_packet_ts(struct pulse_timing *timing, uint64_t now_ns, size_t frames, uint32_t rate)
{
	UNUSED_PARAMETER(timing);

	/* Every packet is stamped as if it had just been captured, so packets
	 * that arrive together all get the same timestamp. */
	return now_ns - util_mul_div64(frames, NSEC_PER_SEC, rate);
}

void pulse_timing_reset(struct pulse_timing *timing)
{
	timing->next_ts = 0;
}
