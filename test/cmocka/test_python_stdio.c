#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <setjmp.h>
#include <cmocka.h>

#include <util/base.h>
#include <util/dstr.h>

#include "obs-scripting-python-startup.h"

/* OBS's Python scripting swaps sys.stdout and sys.stderr for loggers that
 * forward to the script log. Those loggers only had write() and flush(), so a
 * library that probes the stream the usual way, such as sys.stderr.isatty()
 * (youtube_dl, speedtest-cli) or sys.stdout.encoding, raised AttributeError
 * when a script used it (obsproject/obs-studio#4628, reported by vivi90).
 *
 * The test runs OBS's real startup script against a stub obspython module
 * that records what reaches the script log, then checks the interface
 * io.TextIOBase defines for a write-only text stream that has no file
 * descriptor. A failing check prints its traceback through sys.stderr, i.e.
 * into the recorded error log, exactly as it would in OBS. */

static struct dstr info_log;
static struct dstr error_log;

static PyObject *stub_script_log_no_endl(PyObject *self, PyObject *args)
{
	int level;
	const char *msg;
	(void)self;

	if (!PyArg_ParseTuple(args, "is", &level, &msg))
		return NULL;

	dstr_cat(level == LOG_ERROR ? &error_log : &info_log, msg);
	Py_RETURN_NONE;
}

static PyMethodDef stub_methods[] = {
	{"script_log_no_endl", stub_script_log_no_endl, METH_VARARGS, NULL},
	{NULL, NULL, 0, NULL},
};

static struct PyModuleDef stub_module = {
	PyModuleDef_HEAD_INIT, "obspython", NULL, -1, stub_methods, NULL, NULL, NULL, NULL,
};

static PyObject *stub_init(void)
{
	PyObject *module = PyModule_Create(&stub_module);
	if (module) {
		PyModule_AddIntConstant(module, "LOG_ERROR", LOG_ERROR);
		PyModule_AddIntConstant(module, "LOG_WARNING", LOG_WARNING);
		PyModule_AddIntConstant(module, "LOG_INFO", LOG_INFO);
		PyModule_AddIntConstant(module, "LOG_DEBUG", LOG_DEBUG);
	}
	return module;
}

static int setup(void **state)
{
	(void)state;
	if (PyImport_AppendInittab("obspython", stub_init) != 0)
		return -1;
	Py_Initialize();
	if (!Py_IsInitialized() || PyRun_SimpleString(python_startup_script) != 0)
		return -1;
	return 0;
}

static int teardown(void **state)
{
	(void)state;
	Py_Finalize();
	dstr_free(&info_log);
	dstr_free(&error_log);
	return 0;
}

static void clear_logs(void)
{
	dstr_free(&info_log);
	dstr_free(&error_log);
}

/* Runs snippet in OBS's interpreter; on failure shows what reached the
 * script log. */
static void run_snippet(const char *snippet)
{
	clear_logs();
	int ret = PyRun_SimpleString(snippet);
	if (ret != 0 || strstr(error_log.array ? error_log.array : "", "Traceback")) {
		fprintf(stderr, "script error log:\n%s\n", error_log.array ? error_log.array : "");
		fail();
	}
}

static void test_stdio_is_file_like(void **state)
{
	(void)state;
	run_snippet("import io, sys\n"
		    "for stream in (sys.stdout, sys.stderr):\n"
		    "    assert stream.isatty() is False\n"
		    "    assert stream.writable() is True\n"
		    "    assert stream.readable() is False\n"
		    "    assert stream.seekable() is False\n"
		    "    assert isinstance(stream.encoding, str)\n"
		    "    assert isinstance(stream.errors, str)\n"
		    "    assert stream.closed is False\n"
		    "    try:\n"
		    "        stream.fileno()\n"
		    "    except io.UnsupportedOperation:\n"
		    "        pass\n"
		    "    else:\n"
		    "        raise AssertionError('fileno() returned')\n"
		    "    assert stream.write('') == 0\n"
		    "    stream.flush()\n");
}

static void test_stdio_reaches_script_log(void **state)
{
	(void)state;
	run_snippet("import logging, sys\n"
		    "print('to-info')\n"
		    "sys.stderr.write('to-error\\n')\n"
		    "handler = logging.StreamHandler()\n"
		    "logger = logging.getLogger('stdio_probe')\n"
		    "logger.addHandler(handler)\n"
		    "logger.warning('via-logging')\n"
		    "logger.removeHandler(handler)\n");

	if (!info_log.array || strcmp(info_log.array, "to-info\n") != 0 || !error_log.array ||
	    strcmp(error_log.array, "to-error\nvia-logging\n") != 0) {
		fprintf(stderr, "info log: '%s'\nerror log: '%s'\n", info_log.array ? info_log.array : "",
			error_log.array ? error_log.array : "");
		fail();
	}
}

int main(void)
{
	const struct CMUnitTest tests[] = {
		cmocka_unit_test(test_stdio_reaches_script_log),
		cmocka_unit_test(test_stdio_is_file_like),
	};

	return cmocka_run_group_tests(tests, setup, teardown);
}
