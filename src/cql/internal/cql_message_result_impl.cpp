/*
 *      Copyright (C) 2013 DataStax Inc.
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 */
#include <iomanip>
#include <iostream>
#include <sstream>
#include <boost/foreach.hpp>

#include "cql/internal/cql_serialization.hpp"
#include "cql/internal/cql_vector_stream.hpp"
#include "cql/internal/cql_defines.hpp"
#include "cql/internal/cql_list_impl.hpp"
#include "cql/internal/cql_map_impl.hpp"
#include "cql/internal/cql_set_impl.hpp"
#include "cql/cql_uuid.hpp"
	
#include "cql/internal/cql_message_result_impl.hpp"

std::string
result_type_string(cql::cql_short_t t) {
    switch(t) {

    case cql::CQL_RESULT_ROWS:
        return "CQL_RESULT_ROWS";
        break;

    case cql::CQL_RESULT_SET_KEYSPACE:
        return "CQL_RESULT_SET_KEYSPACE";
        break;

    case cql::CQL_RESULT_PREPARED:
        return "CQL_RESULT_PREPARED";
        break;

    case cql::CQL_RESULT_VOID:
        return "CQL_RESULT_VOID";
        break;

    default:
        return "UNKNOWN";
        break;
    }
}

cql::cql_message_result_impl_t::cql_message_result_impl_t() :
    _buffer(new std::vector<cql_byte_t>()),
    _pos(0),
    _row_pos(0),
    _row_count(0),
    _column_count(0),
    _query_id(0),
    _result_type(cql::CQL_RESULT_VOID)
{}

cql::cql_message_result_impl_t::cql_message_result_impl_t(size_t size) :
    _buffer(new std::vector<cql_byte_t>(size)),
    _pos(0),
    _row_pos(0),
    _row_count(0),
    _column_count(0),
    _query_id(0),
    _result_type(cql::CQL_RESULT_VOID)
{}

cql::cql_message_buffer_t
cql::cql_message_result_impl_t::buffer() {
    return _buffer;
}

const cql::cql_result_metadata_t&
cql::cql_message_result_impl_t::get_metadata() {
    return _metadata;
}

cql::cql_result_type_enum
cql::cql_message_result_impl_t::result_type() const {
    return _result_type;
}

cql::cql_opcode_enum
cql::cql_message_result_impl_t::opcode() const {
    return CQL_OPCODE_RESULT;
}

cql::cql_int_t
cql::cql_message_result_impl_t::size() const {
    return _buffer->size();
}

std::string
cql::cql_message_result_impl_t::str() const {
    std::stringstream output;
    output << std::string("RESULT ") << result_type_string(_result_type);

    if (! _query_id.empty()) {
        output << " QUERY_ID 0x";
        output << std::setfill('0');
        BOOST_FOREACH(cql::cql_byte_t c, _query_id) {
            output << std::setw(2) << cql::hex(c);
        }
    }
    output << " ROW_COUNT " << _row_count;
    output << " METADATA " << _metadata.str();
    return output.str();
}

bool
cql::cql_message_result_impl_t::consume(cql::cql_error_t*) {
    _keyspace_name.clear();
    _row_count = 0;
    _row_pos = 0;
    _pos = &((*_buffer)[0]);

    cql::cql_int_t result_type = 0;
    _pos = cql::decode_int(_pos, result_type);

    switch (result_type) {

    case 0x0001:
        _result_type = cql::CQL_RESULT_VOID;
        break;

    case 0x0002:
        _result_type = cql::CQL_RESULT_ROWS;
        break;

    case 0x0003:
        _result_type = cql::CQL_RESULT_SET_KEYSPACE;
        break;

    case 0x0004:
        _result_type = cql::CQL_RESULT_PREPARED;
        break;

    case 0x0005:
        _result_type = cql::CQL_RESULT_SCHEMA_CHANGE;
        break;
    }

    switch(_result_type) {

    case CQL_RESULT_ROWS:
        _pos = _metadata.read(_pos);
        _pos = cql::decode_int(_pos, _row_count);
        _column_count = _metadata.column_count();
        _row.resize(_column_count);
        break;

    case CQL_RESULT_SET_KEYSPACE:
        cql::decode_string(_pos, _keyspace_name);
        _row_pos = _row_count;
        break;

    case CQL_RESULT_PREPARED:
        cql::decode_short_bytes(_pos, _query_id);
        _row_pos = _row_count;
        break;

    case CQL_RESULT_VOID:
        _row_pos = _row_count;
        break;

    case CQL_RESULT_SCHEMA_CHANGE:
        break;
    }

    return true;
}

bool
cql::cql_message_result_impl_t::prepare(cql::cql_error_t*) {
    return true;
}

size_t
cql::cql_message_result_impl_t::row_count() const {
    return _row_count;
}

size_t
cql::cql_message_result_impl_t::column_count() const {
    return _column_count;
}

const std::vector<cql::cql_byte_t>&
cql::cql_message_result_impl_t::query_id() const {
    return _query_id;
}

bool
cql::cql_message_result_impl_t::exists(const std::string& column) const {
    return _metadata.exists(column);
}

bool
cql::cql_message_result_impl_t::column_name(int i,
                                            std::string& output_keyspace,
                                            std::string& output_table,
                                            std::string& output_column) const {
    return _metadata.column_name(i,
                                 output_keyspace,
                                 output_table,
                                 output_column);
}

bool
cql::cql_message_result_impl_t::column_class(int i,
        std::string& output) const {
    return _metadata.column_class(i, output);
}

bool
cql::cql_message_result_impl_t::column_class(const std::string& column,
        std::string& output) const {
    return _metadata.column_class(column, output);
}

bool
cql::cql_message_result_impl_t::column_type(int i,
        cql_column_type_enum& output) const {
    return _metadata.column_type(i, output);
}

bool
cql::cql_message_result_impl_t::column_type(const std::string& column,
        cql_column_type_enum& output) const {
    return _metadata.column_type(column, output);
}

bool
cql::cql_message_result_impl_t::get_index(const std::string& column,
        int& output) const {
    return _metadata.get_index(column, output);
}

bool
cql::cql_message_result_impl_t::next() {
    if (_row_pos >= (unsigned) _row_count) {
        return false;
    }
    _row.clear();

    for (int i = 0; i < _column_count; ++i) {
        _row.push_back(_pos);
        cql::cql_int_t len = 0;
        _pos = cql::decode_int(_pos, len);
        if (len > 0) {
            _pos += len;
        }
    }

    _row_pos++;
    return true;
}

bool
cql::cql_message_result_impl_t::get_nullity(
    int   i,
    bool& output) const {
    if (i >= _column_count || i < 0) {
        return false;
    }
    cql::cql_int_t row_size = 0;
    cql::decode_int(_row[i], row_size);
    output = (row_size <= 0);
    return true;
}

bool
cql::cql_message_result_impl_t::get_nullity(
    const std::string& column,
    bool&              output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        cql::cql_int_t row_size = 0;
        cql::decode_int(_row[i], row_size);
        output = (row_size <= 0);
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_bool(int i,
        bool& output) const {
    if (is_valid(i, cql::CQL_COLUMN_TYPE_BOOLEAN)) {
        output = *(_row[i] + sizeof(cql_int_t)) != 0x00;
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_bool(const std::string& column,
        bool& output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_bool(i, output);
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_int(int i,
                                        cql::cql_int_t& output) const {
    if (is_valid(i, cql::CQL_COLUMN_TYPE_INT)) {
        cql::decode_int(_row[i] + sizeof(cql_int_t), output);
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_int(const std::string& column,
                                        cql::cql_int_t& output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_int(i, output);
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_float(int i,
        float& output) const {
    if (is_valid(i, cql::CQL_COLUMN_TYPE_FLOAT)) {
        cql::decode_float(_row[i] + sizeof(cql_int_t), output);
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_float(const std::string& column,
        float& output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_float(i, output);
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_double(int i,
        double& output) const {
    if (is_valid(i, cql::CQL_COLUMN_TYPE_DOUBLE)) {
        cql_byte_t* p = _row[i] + sizeof(cql_int_t);
        cql::decode_double(p, output);
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_double(const std::string& column,
        double& output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_double(i, output);
    }
    return false;
}		

bool	
cql::cql_message_result_impl_t::get_bigint(int i,
        cql::cql_bigint_t& output) const {
    if (is_valid(i, cql::CQL_COLUMN_TYPE_BIGINT)
          || is_valid(i, cql::CQL_COLUMN_TYPE_TIMESTAMP)
          || is_valid(i, cql::CQL_COLUMN_TYPE_COUNTER)) {
        cql::decode_bigint(_row[i] + sizeof(cql_int_t), output);
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_bigint(const std::string& column,
        cql::cql_bigint_t& output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_bigint(i, output);
    }
    return false;
}

bool	
cql::cql_message_result_impl_t::get_timestamp(int i,
        cql::cql_bigint_t& output) const {
    if( is_valid(i, cql::CQL_COLUMN_TYPE_TIMESTAMP) ) {
        cql::decode_bigint(_row[i] + sizeof(cql_int_t), output);
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_timestamp(const std::string& column,
        cql::cql_bigint_t& output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_timestamp(i, output);
    }
    return false;
}		
			
bool	
cql::cql_message_result_impl_t::get_timeuuid(int i,
	cql::cql_bigint_t& output) const {
	std::vector< cql::cql_byte_t > v;
	if( !get_data( i, v ) )
		return false;

	if( v.size() != 16 )
		return false;
		
	cql::cql_bigint_t result(0);	
				
	static int indexes[] = {6,7,4,5,0,1,2,3};		//// take bytes of the array in this order.		
				
	for(int i = 0; i < 8; ++i) {		
		result = result << 8;	
		int const index = indexes[i];
		cql::cql_bigint_t t1 = static_cast< cql::cql_bigint_t >(v[index]);
			
		if(index == 6)			//// the four most significant bits are the version. Ignore these bits.		
			t1 = t1 & 0x0F;		//// take only half of this byte. 
			
		result = result | t1;			
	}		
			
	output = result;
	return true;			
}				
				
bool			
cql::cql_message_result_impl_t::get_timeuuid(const std::string& column,
	cql::cql_bigint_t& output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_timeuuid(i, output);
    }	
    return false;
}		

bool
cql::cql_message_result_impl_t::get_counter(int i,
               cql::cql_bigint_t& output) const {
	return get_bigint( i, output );
}

bool
cql::cql_message_result_impl_t::get_counter(const std::string& column,
               cql::cql_bigint_t& output) const {
	return get_bigint( column, output );
}
	
bool
cql::cql_message_result_impl_t::get_string(int i,
        std::string& output) const {
    cql_byte_t* data = 0;
    cql_int_t size = 0;
    if (get_data(i, &data, size)) {
        output.assign(data, data + size);
        return true;
    }
    return false;
}
		
bool
cql::cql_message_result_impl_t::get_string(const std::string& column,
        std::string& output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_string(i, output);
    }
    return false;
}		
		
bool	
cql::cql_message_result_impl_t::get_ascii(int i,
               std::string& output) const {
	return get_string( i, output );
}		
		
bool		
cql::cql_message_result_impl_t::get_ascii(const std::string& column,
               std::string& output) const {
	return get_string( column, output );
}
	
bool	
cql::cql_message_result_impl_t::get_varchar(int i,
               std::string& output) const {
	return get_string( i, output );
}

bool	
cql::cql_message_result_impl_t::get_varchar(const std::string& column,
               std::string& output) const {
	return get_string( column, output );
}
	
bool
cql::cql_message_result_impl_t::get_data(int i,
        cql::cql_byte_t** output,
        cql::cql_int_t& size) const {
    bool empty = false;
    if (get_nullity(i, empty)) {
        if (!empty) {
            cql_byte_t* pos = _row[i];
            *output = cql::decode_int(pos, size);
            return true;
        }
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_data(const std::string& column,
        cql::cql_byte_t** output,
        cql::cql_int_t& size) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_data(i, output, size);
    }
    return false;
}			

bool						 
cql::cql_message_result_impl_t::get_data( int i,
										  std::vector< cql::cql_byte_t > & output ) const {
	cql::cql_byte_t* output_ptr = NULL;

	bool empty = false;
    if (get_nullity(i, empty)) {
        if (!empty) {
            cql_byte_t* pos = _row[i];
			cql::cql_int_t size( 0 );
            output_ptr = cql::decode_int(pos, size);	
				
			output.resize( size );
			for( int i = 0; i < size; ++i ) {	
				output[ i ] = *(output_ptr + i);	
			}	
				
            return true;
        }		
    }			
}				
				
bool								
cql::cql_message_result_impl_t::get_data( const std::string & column,
										  std::vector< cql::cql_byte_t > & output ) const {
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_data(i, output );
    }
    return false;
}
			
bool
cql::cql_message_result_impl_t::get_list(int i,
        cql::cql_list_t** output) const {
    if (is_valid(i, cql::CQL_COLUMN_TYPE_LIST)) {
        cql::cql_column_type_enum member_type;
        std::string member_class;
        _metadata.collection_primary_type(i, member_type);
        _metadata.collection_primary_class(i, member_class);
        *output = new cql::cql_list_impl_t(_row[i] + sizeof(cql_int_t), member_type, member_class);
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_list(int i,
        boost::shared_ptr< cql::cql_list_t > & output) const {
    if (is_valid(i, cql::CQL_COLUMN_TYPE_LIST)) {
        cql::cql_column_type_enum member_type;
        std::string member_class;
        _metadata.collection_primary_type(i, member_type);
        _metadata.collection_primary_class(i, member_class);
        output.reset( new cql::cql_list_impl_t(_row[i] + sizeof(cql_int_t), member_type, member_class) );
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_list(const std::string& column,
        cql::cql_list_t** output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_list(i, output);
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_list(const std::string& column,
        boost::shared_ptr< cql::cql_list_t > & output) const {	
    int i = 0;	
    if (_metadata.get_index(column, i)) {
        return get_list(i, output);
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_set(int i,
                                        cql::cql_set_t** output) const {
    if (is_valid(i, cql::CQL_COLUMN_TYPE_SET)) {
        cql::cql_column_type_enum member_type;
        std::string member_class;
        _metadata.collection_primary_type(i, member_type);
        _metadata.collection_primary_class(i, member_class);
        *output = new cql::cql_set_impl_t(_row[i] + sizeof(cql_int_t), member_type, member_class);
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_set(int i,	
                                        boost::shared_ptr< cql::cql_set_t > & output) const {
    if (is_valid(i, cql::CQL_COLUMN_TYPE_SET)) {
        cql::cql_column_type_enum member_type;
        std::string member_class;
        _metadata.collection_primary_type(i, member_type);
        _metadata.collection_primary_class(i, member_class);	
        output.reset( new cql::cql_set_impl_t(_row[i] + sizeof(cql_int_t), member_type, member_class) );
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_set(const std::string& column,
                                        cql::cql_set_t** output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_set(i, output);
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_set(const std::string& column,	
                                        boost::shared_ptr< cql::cql_set_t > & output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_set(i, output);
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_map(int i,
                                        cql::cql_map_t** output) const {
    if (is_valid(i, cql::CQL_COLUMN_TYPE_MAP)) {
        cql::cql_column_type_enum key_type;
        cql::cql_column_type_enum value_type;
        std::string key_class;
        std::string value_class;
        _metadata.collection_primary_type(i, key_type);
        _metadata.collection_primary_class(i, key_class);
        _metadata.collection_secondary_type(i, value_type);
        _metadata.collection_secondary_class(i, value_class);
        *output = new cql::cql_map_impl_t(_row[i] + sizeof(cql_int_t), key_type, value_type, key_class, value_class);
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_map(int i,	
                                        boost::shared_ptr< cql::cql_map_t > & output) const {
    if (is_valid(i, cql::CQL_COLUMN_TYPE_MAP)) {
        cql::cql_column_type_enum key_type;
        cql::cql_column_type_enum value_type;
        std::string key_class;
        std::string value_class;
        _metadata.collection_primary_type(i, key_type);
        _metadata.collection_primary_class(i, key_class);
        _metadata.collection_secondary_type(i, value_type);
        _metadata.collection_secondary_class(i, value_class);
        output.reset( new cql::cql_map_impl_t(_row[i] + sizeof(cql_int_t), key_type, value_type, key_class, value_class) );	
        return true;
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_map(const std::string& column,
                                        cql::cql_map_t** output) const {
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_map(i, output);
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_map(const std::string& column,	
                                        boost::shared_ptr< cql::cql_map_t > & output) const {	
    int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_map(i, output);
    }
    return false;
}

bool
cql::cql_message_result_impl_t::get_keyspace_name(std::string& output) const {
    output = _keyspace_name;
    return true;
}			
			
bool		
cql::cql_message_result_impl_t::get_uuid( int i,
										  cql_uuid_t& output ) const
{
    std::vector<cql_byte_t> buffer;
	bool ret_val = get_data(i, buffer);
    output = cql_uuid_t(buffer);
    return ret_val && (buffer.size() == 16);
}
				
bool
cql::cql_message_result_impl_t::get_uuid( const std::string& column,
										  cql_uuid_t& output ) const {
    std::vector<cql_byte_t> buffer;
	bool ret_val = get_data(column, buffer);
    output = cql_uuid_t(buffer);
    return ret_val && (buffer.size() == 16);
}		
				
bool	
cql::cql_message_result_impl_t::get_uuid( int i,
										  std::string & output ) const {
	cql_uuid_t uuid_out;
    bool ret_val = get_uuid(i, uuid_out);
    output = uuid_out.to_string();
    return ret_val;
}
				
bool
cql::cql_message_result_impl_t::get_uuid( const std::string& column,
										  std::string & output ) const {
    cql_uuid_t uuid_out;
    bool ret_val = get_uuid(column, uuid_out);
    output = uuid_out.to_string();
    return ret_val;
}		
			
bool //// is it possible to convert the DECIMAL to int ( 32 bits ) without rounding.
cql::cql_message_result_impl_t::get_decimal_is_int( std::string const & column ) const { 
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_decimal_is_int(i);
    }
    return false;
}

bool //// is it possible to convert the DECIMAL to int64 ( 64 bits ) without rounding.	
cql::cql_message_result_impl_t::get_decimal_is_int_64( std::string const & column ) const { 
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_decimal_is_int_64(i);
    }	
    return false;
}

bool 	//// is it possible to convert the DECIMAL to double even with small roundings error.
cql::cql_message_result_impl_t::get_decimal_is_double( std::string const & column ) const {	
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_decimal_is_double(i);
    }	
    return false;
}
				
bool 
cql::cql_message_result_impl_t::get_decimal_int( std::string const & column, 
	                 cql::cql_int_t & output ) const {				//// convert DECIMAL to 32-int if it is possible
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_decimal_int(i, output);
    }
    return false;
}

bool 
cql::cql_message_result_impl_t::get_decimal_int_64( std::string const & column, 
	                    cql::cql_bigint_t & output ) const {		//// convert DECIMAL to 64-int if it is possible
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_decimal_int_64(i, output);
    }
    return false;
}

bool 
cql::cql_message_result_impl_t::get_decimal_double( std::string const & column, 
	                    double & output ) const {					//// convert DECIMAL to double with roundings error.	
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_decimal_double(i, output);
    }
    return false;
}
		
bool		
cql::cql_message_result_impl_t::get_decimal_is_int( int i ) const {			//// is it possible to convert the DECIMAL to int ( 32 bits ) without rounding.		
	std::vector< cql::cql_byte_t > v;
	if( !get_data( i, v ) )
		return false;
			
	if( v.size() < 5 )
		return false;		//// there must be at least four bytes in the vector.	
			
	if( v[ 0 ] != 0 || v[ 1 ] != 0 || v[ 2 ] != 0 || v[ 3 ] != 0 )		//// the first four bytes must be zero. 
		return false;		

	return( v.size() <= 8 );	// it will be convertible to int32 if there less than 8 bytes in the vector. 		
}		
					
bool	
cql::cql_message_result_impl_t::get_decimal_is_int_64( int i ) const {		//// is it possible to convert the DECIMAL to int64 ( 64 bits ) without rounding.	
	std::vector< cql::cql_byte_t > v;
	if( !get_data( i, v ) )
		return false;
			
	if( v.size() < 5 )
		return false;		//// there must be at least four bytes in the vector.	
			
	if( v[ 0 ] != 0 || v[ 1 ] != 0 || v[ 2 ] != 0 || v[ 3 ] != 0 )		//// the first four bytes must be zero. 
		return false;		

	return( v.size() <= 12 );	// it will be convertible to int32 if there less than 12 bytes in the vector. 
}				
								
bool			
cql::cql_message_result_impl_t::get_decimal_is_double( int i ) const {		//// is it possible to convert the DECIMAL to double even with small roundings error.
	std::vector< cql::cql_byte_t > v;
	if( !get_data( i, v ) )
		return false;
			
	if( v.size() < 5 )
		return false;		//// there must be at least four bytes in the vector.	
			
	if( v[ 0 ] != 0 || v[ 1 ] != 0 || v[ 2 ] != 0 )		//// the first three bytes must be zero. 
		return false;		

	if( v.size() == 5 && v[ 4 ] == 0 ) 
		return true;		//// the value inside is exactly zero.	
					
	if( v.size() > 12 )		//// the data consists of more than 64 bits. It will not be convertible to double.;	
		return false;

	return true;
}			
						
bool		
cql::cql_message_result_impl_t::get_decimal_int( int i, 
												 cql::cql_int_t & output ) const {				//// convert DECIMAL to 32-int if it is possible
	std::vector< cql::cql_byte_t > v;
	if( !get_data( i, v ) )
		return false;
			
	if( v.size() < 5 )
		return false;		//// there must be at least four bytes in the vector.	
			
	if( v[ 0 ] != 0 || v[ 1 ] != 0 || v[ 2 ] != 0 || v[ 3 ] != 0 )		//// the first four bytes must be zero. 
		return false;		
		
	if( v.size() > 8 )		//// the data consists of more than 32 bits. It will not be convertible to int32;	
		return false;
				
	unsigned char const first_digit = v[ 4 ];		//// take the first digit of the data bytes.
	unsigned char const last_byte_first_digit = first_digit & 0x80;		//// take the most meaning bit of the first byte.
				
	unsigned char arr[ 4 ];				//// temporary array for making conversion to int. 

	unsigned char byte_for_filling( 0x00 );		//// the byte for filling in the more significant bytes. 
		
	if( last_byte_first_digit == 0x80 ) {	//// the most meaning bit of the first byte is set to 1. It means that the value is NEGATIVE. 
		byte_for_filling = 0xFF;		//// the filing will be with 0xFF value.
	}		
			
	int const number_of_bytes_for_filling( 8 - v.size() );
			
	for( int i = 0; i < number_of_bytes_for_filling; ++i ) {		
		arr[ i ] = byte_for_filling;		
	}			
				
	int const number_of_bytes_to_copy( v.size() - 4 );	
				
	for( int i = 0; i < number_of_bytes_to_copy; ++i ) {		
		arr[ i + number_of_bytes_for_filling ] = v[ i + 4 ];	
	}		
			
	int result( 0 );
			
	for( int i = 0;  i < 4; ++i ) {
		result = result << 8;	
		result = result | static_cast< int >( arr[ i ] );
	}		

	output = result;									
	return true;
}			
				
bool		
cql::cql_message_result_impl_t::get_decimal_int_64( int i, 
													cql::cql_bigint_t & output ) const {		//// convert DECIMAL to 64-int if it is possible
	std::vector< cql::cql_byte_t > v;
	if( !get_data( i, v ) )
		return false;
			
	if( v.size() < 5 )
		return false;		//// there must be at least four bytes in the vector.	
			
	if( v[ 0 ] != 0 || v[ 1 ] != 0 || v[ 2 ] != 0 || v[ 3 ] != 0 )		//// the first four bytes must be zero. 
		return false;		
		
	if( v.size() > 12 )		//// the data consists of more than 64 bits. It will not be convertible to int64;	
		return false;
				
	unsigned char const first_digit = v[ 4 ];		//// take the first digit of the data bytes.
	unsigned char const last_byte_first_digit = first_digit & 0x80;		//// take the most meaning bit of the first byte.
	
	unsigned char arr[ 8 ];				//// temporary array for making conversion to int. 

	unsigned char byte_for_filling( 0x00 );		//// the byte for filling in the more significant bytes. 

	if( last_byte_first_digit == 0x80 ) {	//// the most meaning bit of the first byte is set to 1. It means that the value is NEGATIVE. 
		byte_for_filling = 0xFF;		//// the filing will be with 0xFF value.
	}		
			
	int const number_of_bytes_for_filling( 12 - v.size() );
			
	for( int i = 0; i < number_of_bytes_for_filling; ++i ) {		
		arr[ i ] = byte_for_filling;		
	}			
				
	int const number_of_bytes_to_copy( v.size() - 4 );	
					
	for( int i = 0; i < number_of_bytes_to_copy; ++i ) {		
		arr[ i + number_of_bytes_for_filling ] = v[ i + 4 ];	
	}		
			
	cql::cql_bigint_t result( 0 );
			
	for( int i = 0;  i < 8; ++i ) {
		result = result << 8;	
		result = result | static_cast< int >( arr[ i ] );
	}		
			
	output = result;						
	return true;
}			
			
	
bool 
cql::cql_message_result_impl_t::get_decimal_double( int i, 
													double & output ) const {					//// convert DECIMAL to double with roundings error.	
	std::vector< cql::cql_byte_t > v;
	if( !get_data( i, v ) )
		return false;
			
	if( v.size() < 5 )
		return false;		//// there must be at least four bytes in the vector.	
			
	if( v[ 0 ] != 0 || v[ 1 ] != 0 || v[ 2 ] != 0 )		//// the first three bytes must be zero. 
		return false;		

	if( v.size() == 5 && v[ 4 ] == 0 ) {
		output = 0.0;		//// the first bytes are zero. This is ZERO. 
		return true;		
	}	
			
	if( v.size() > 12 )		//// the data consists of more than 64 bits. It will not be convertible to double.;	
		return false;
				
	unsigned char const first_digit = v[ 4 ];								//// take the first digit of the data bytes.
	unsigned char const last_byte_first_digit = first_digit & 0x80;		//// take the most meaning bit of the first byte.
	
	unsigned char arr[ 8 ];						//// temporary array for making conversion to int64.	 

	unsigned char byte_for_filling( 0x00 );		//// the byte for filling in the more significant bytes. 

	if( last_byte_first_digit == 0x80 ) {	//// the most meaning bit of the first byte is set to 1. It means that the value is NEGATIVE. 
		byte_for_filling = 0xFF;		//// the filing will be with 0xFF value.
	}		
			
	int const number_of_bytes_for_filling( 12 - v.size() );
			
	for( int j = 0; j < number_of_bytes_for_filling; ++j ) {		
		arr[ j ] = byte_for_filling;		
	}			
				
	int const number_of_bytes_to_copy( v.size() - 4 );	
					
	for( int j = 0; j < number_of_bytes_to_copy; ++j ) {		
		arr[ j + number_of_bytes_for_filling ] = v[ j + 4 ];	
	}			
			
	cql::cql_bigint_t result( 0 );
			
	for( int j = 0;  j < 8; ++j ) {
		result = result << 8;	
		result = result | static_cast< int >( arr[ j ] );
	}		

	unsigned char const number_of_position_after_dot( v[ 3 ] );
		
	double const f1 = static_cast< double >( result );

	if( number_of_position_after_dot == 0 ) {
		output = f1;
		return true;
	}		
			
	output = f1 / pow( 10.0, number_of_position_after_dot );									
	return true;			
}			

bool 
cql::cql_message_result_impl_t::get_varint( std::string const & column, 
	                    cql::cql_bigint_t & output ) const {			//// convert varint to int64.	
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_varint(i, output);
    }
    return false;
}			
			
bool		
cql::cql_message_result_impl_t::get_varint( int i, 
	            cql::cql_bigint_t & output ) const {					//// convert varint to int64.

	std::vector< cql::cql_byte_t > v;
	if( !get_data( i, v ) )
		return false;

	std::size_t const t = v.size();
		
	if( t < 1 || t > 8 )
		return false;		
		
	unsigned char const first_digit = v[ 0 ];		//// take the first digit of the data bytes.
	unsigned char const last_byte_first_digit = first_digit & 0x80;		//// take the most meaning bit of the first byte.
		
	unsigned char arr[ 8 ];				//// temporary array for making conversion to int. 

	unsigned char byte_for_filling( 0x00 );		//// the byte for filling in the more significant bytes. 

	if( last_byte_first_digit == 0x80 ) {	//// the most meaning bit of the first byte is set to 1. It means that the value is NEGATIVE. 
		byte_for_filling = 0xFF;		//// the filing will be with 0xFF value.
	}		
			
	int const number_of_bytes_for_filling( 8 - v.size() );
			
	for( int j = 0; j < number_of_bytes_for_filling; ++j ) {		
		arr[ j ] = byte_for_filling;		
	}			
					
	int const number_of_bytes_to_copy( v.size() );	
				
	for( i = 0; i < number_of_bytes_to_copy; ++i ) {			
		arr[ i + number_of_bytes_for_filling ] = v[ i ];	
	}		
			
	cql::cql_bigint_t result( 0 );
			
	for( int i = 0;  i < 8; ++i ) {
		result = result << 8;	
		result = result | static_cast< int >( arr[ i ] );
	}		
			
	output = result;		
	return true;	
}			
					
bool		
cql::cql_message_result_impl_t::get_blob( int i, 
										  std::vector< cql::cql_byte_t > & output ) const {	//// return blob as vector. Data is copied
 	return get_data( i, output );
}	
					
bool	
cql::cql_message_result_impl_t::get_blob( std::string const & column, 
									      std::vector< cql::cql_byte_t > & output ) const {	//// return blob as vector. Data is copied
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_blob(i, output);
    }
    return false;
}

bool 
cql::cql_message_result_impl_t::get_blob( int i,
										  std::pair< cql::cql_byte_t *, cql::cql_int_t > & output ) const {		//// return blob as pure pointer and size. Data is not copied
	cql::cql_byte_t* output_ptr = NULL;
	bool empty = false;
    if (get_nullity(i, empty)) {
        if (!empty) {
            cql_byte_t* pos = _row[i];
			cql::cql_int_t size( 0 );
            output_ptr = cql::decode_int(pos, size);	
			output.first = output_ptr;
			output.second = size;
            return true;
        }		
    }			
	return false;
}
					
bool	
cql::cql_message_result_impl_t::get_blob( std::string const & column, 
										  std::pair< cql::cql_byte_t *, cql::cql_int_t > & output ) const {		//// return blob as pure pointer and size. Data is not copied		
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_blob(i, output);
    }
    return false;
}		
			
bool
cql::cql_message_result_impl_t::get_text(int i,
               std::string& output) const {
	return get_ascii( i, output );
}
				
bool
cql::cql_message_result_impl_t::get_text(const std::string& column,
               std::string& output) const {
	return get_ascii( column, output );
}		
			
bool			
cql::cql_message_result_impl_t::get_inet(std::string const& column, 
										 /*cql_host_t::ip_address_t*/ boost::asio::ip::address & output) const {			//// return inet.
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_inet(i, output);
    }
    return false;
}
			
bool	
cql::cql_message_result_impl_t::get_inet(int i, 
			 /*cql_host_t::ip_address_t*/ boost::asio::ip::address & output) const {			//// return inet.
	cql_byte_t* output_ptr(NULL);
    bool empty = false;
    if (get_nullity(i, empty)) {
        if (!empty) {
			cql::cql_int_t size(0);
            cql_byte_t* pos = _row[i];
            output_ptr = cql::decode_int(pos, size);

			if(size == 4) {  // IPv4
				std::string const ip_str = cql::decode_ipv4_from_bytes(output_ptr);					
				output = /*cql::cql_host_t::ip_address_t*/boost::asio::ip::address::from_string(ip_str);
				return true;
			}
			else if(size == 16) {	// IPv6		
				std::string const ip_str = cql::decode_ipv6_from_bytes(output_ptr);
				output = /*cql::cql_host_t::ip_address_t*/boost::asio::ip::address::from_string(ip_str);
				return true;
			}
		}
	}
	return false;	
}	
		
bool
cql::cql_message_result_impl_t::get_varint(std::string const& column, 
										   boost::multiprecision::cpp_int & output) const			//// return boost multiprecision cpp_int
{
	int i = 0;
    if (_metadata.get_index(column, i)) {
        return get_varint(i, output);
    }
    return false;
}

bool 
cql::cql_message_result_impl_t::get_varint(int i,		
										   boost::multiprecision::cpp_int & output) const			//// return boost multiprecision cpp_int
{		
	if(!is_valid(i,cql::CQL_COLUMN_TYPE_VARINT)) 
		return false;

	std::vector<cql::cql_byte_t> v;
	if(!get_data(i,v))
		return false;

	std::size_t const t = v.size();
		
	if(t < 1)
		return false;		

	if(t == 1 && v[0] == 0) {
		output = 0;
		return true;
	}
		
	unsigned char const first_digit = v[0];								//// take the first digit of the data bytes.
	unsigned char const last_byte_first_digit = first_digit & 0x80;		//// take the most meaning bit of the first byte.

	boost::multiprecision::cpp_int r(0);

	if(last_byte_first_digit == 0x80) {					//// the most meaning bit of the first byte is set to 1. It means that the value is NEGATIVE. 		
		std::vector<cql::cql_byte_t> v1 = v;

		for(int i = 0; i < v1.size(); ++i)
			v1[i] = v1[i] ^ 0xFF;						//// negate all bits to negate the value

		for( int i = 0; i < t; ++i ) {
			r = r << 8;
			boost::multiprecision::cpp_int k = v1[i];	
			r = r | k;
		}	
				
		r = r + 1;			
		boost::multiprecision::cpp_int const r_neg(-1);
		r = r * r_neg;				
	}		
	else	
	{	// the number is POSITIVE.
		for( int i = 0; i < t; ++i ) {
			r = r << 8;
			boost::multiprecision::cpp_int k = v[i];	
			r = r | k;
		}	
	}		
	
	output = r;	
	return true;							
}
		