#include <ntifs.h>
#include <stdarg.h>
#include "logger.hxx"

void Logger::Output(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vDbgPrintExWithPrefix("[DRIVER] ", 0, 0, format, args);
	va_end(args);
}

void Logger::Status(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	vDbgPrintExWithPrefix("[STATUS] ", 0, 0, format, args);
	va_end(args);
}
