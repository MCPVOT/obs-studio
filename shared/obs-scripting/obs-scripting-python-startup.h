/******************************************************************************
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
******************************************************************************/

#pragma once

/* Run once obspython is importable: sends sys.stdout and sys.stderr to the
 * script log. The loggers are write-only text streams without a file
 * descriptor, so code that probes the stream (isatty(), encoding, fileno())
 * works. Kept in its own header so test/cmocka can run it against a stub
 * obspython module. */
static const char python_startup_script[] = "\n\
import sys\n\
import os\n\
import io\n\
import obspython\n\
class script_log_stream(io.TextIOBase):\n\
	level = obspython.LOG_INFO\n\
	encoding = 'utf-8'\n\
	errors = 'strict'\n\
	def writable(self):\n\
		return True\n\
	def write(self, message):\n\
		obspython.script_log_no_endl(self.level, message)\n\
		return len(message)\n\
	def flush(self):\n\
		pass\n\
class stdout_logger(script_log_stream):\n\
	pass\n\
class stderr_logger(script_log_stream):\n\
	level = obspython.LOG_ERROR\n\
	errors = 'backslashreplace'\n\
os.environ['PYTHONUNBUFFERED'] = '1'\n\
sys.stdout = stdout_logger()\n\
sys.stderr = stderr_logger()\n";
