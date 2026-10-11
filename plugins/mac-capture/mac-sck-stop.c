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
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include "mac-sck-stop.h"

/* RED stub: the destroy paths do not wait for the stop completion yet and
 * always free the stop event. */
enum sck_stop_wait_result sck_wait_for_stop(os_event_t *stop_completed, unsigned int timeout_ms)
{
	(void)stop_completed;
	(void)timeout_ms;
	return SCK_STOP_WAIT_COMPLETED;
}

bool sck_stop_may_destroy_event(enum sck_stop_wait_result result)
{
	(void)result;
	return true;
}
