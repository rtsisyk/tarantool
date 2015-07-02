/*
 * Redistribution and use in source and binary forms, with or
 * without modification, are permitted provided that the following
 * conditions are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the
 *    following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY <COPYRIGHT HOLDER> ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * <COPYRIGHT HOLDER> OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
 * BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#include "exception.h"
#include "say.h"
#include "fiber.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>

/** out_of_memory::size is zero-initialized by the linker. */
static OutOfMemory out_of_memory(__FILE__, __LINE__,
				 sizeof(OutOfMemory), "malloc", "exception");

struct diag *
diag_get()
{
	return &fiber()->diag;
}

static const struct method exception_methods[] = {
	make_method(&type_Exception, "message", &Exception::errmsg),
	make_method(&type_Exception, "file", &Exception::file),
	make_method(&type_Exception, "line", &Exception::line),
	make_method(&type_Exception, "log", &Exception::log),
	METHODS_END
};
const struct type type_Exception = make_type("Exception", NULL,
	exception_methods);

void *
Exception::operator new(size_t size)
{
	void *buf = malloc(size);
	if (buf != NULL)
		return buf;
	diag_add_error(&fiber()->diag, &out_of_memory);
	throw &out_of_memory;
}

void
Exception::operator delete(void *ptr)
{
	free(ptr);
}

Exception::~Exception()
{
	if (this != &out_of_memory) {
		assert(m_ref == 0);
	}
}

Exception::Exception(const struct type *type_arg, const char *file,
	unsigned line)
	: Object(), type(type_arg), m_ref(0) {
	if (m_file != NULL) {
		snprintf(m_file, sizeof(m_file), "%s", file);
		m_line = line;
	} else {
		m_file[0] = 0;
		m_line = 0;
	}
	m_errmsg[0] = 0;
	if (this == &out_of_memory) {
		/* A special workaround for out_of_memory static init */
		out_of_memory.m_ref = 1;
		return;
	}
}

void
Exception::log() const
{
	_say(S_ERROR, m_file, m_line, m_errmsg, "%s", type->name);
}

static const struct method systemerror_methods[] = {
	make_method(&type_SystemError, "errnum", &SystemError::errnum),
	METHODS_END
};

const struct type type_SystemError = make_type("SystemError", &type_Exception,
	systemerror_methods);
SystemError::SystemError(const struct type *type, const char *file, unsigned line)
	:Exception(type, file, line),
	  m_errno(errno)
{
	/* nothing */
}

SystemError::SystemError(const char *file, unsigned line,
			 const char *format, ...)
	: Exception(&type_SystemError, file, line),
	m_errno(errno)
{
	va_list ap;
	va_start(ap, format);
	init(format, ap);
	va_end(ap);
}

void
SystemError::init(const char *format, ...)
{
	va_list ap;
	va_start(ap, format);
	init(format, ap);
	va_end(ap);
}

void
SystemError::init(const char *format, va_list ap)
{
	vsnprintf(m_errmsg, sizeof(m_errmsg), format, ap);
}

void
SystemError::log() const
{
	_say(S_SYSERROR, m_file, m_line, strerror(m_errno), "SystemError %s",
	     m_errmsg);
}

const struct type type_OutOfMemory =
	make_type("OutOfMemory", &type_Exception);
OutOfMemory::OutOfMemory(const char *file, unsigned line,
			 size_t amount, const char *allocator,
			 const char *object)
	: SystemError(&type_OutOfMemory, file, line)
{
	m_errno = ENOMEM;
	snprintf(m_errmsg, sizeof(m_errmsg),
		 "Failed to allocate %u bytes in %s for %s",
		 (unsigned) amount, allocator, object);
}
