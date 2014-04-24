/*
 * File:   cql_exception.hpp
 * Author: mc
 *
 * Created on September 11, 2013, 5:53 PM
 */

#ifndef CQL_EXCEPTION_HPP
#define	CQL_EXCEPTION_HPP

#include <cstdlib>
#include <string>
#include <exception>

#include "cql/cql_config.hpp"

namespace cql {

// Base class for all exceptions thrown by the driver.
class CQL_EXPORT cql_exception: public std::exception {
public:
    // Constructor version for STATIC strings
    cql_exception(const char* message);

	// Constructor version for dynamically generated strings
	cql_exception(const std::string& message);

    // cql_exception must be copyable to be used with boost::promises.
    // It allocates memory, so custom copy ctor is needed. I'm sorry.
    cql_exception(const cql_exception&);
    
	virtual ~cql_exception() throw();

	// Returns text message that describes exception.
	virtual const char* what() const throw();

private:
	const char* _buffer;
    bool        _buffer_allocated;
};

}

#endif	/* CQL_EXCEPTION_HPP */

