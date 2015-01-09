/**
 *  @file result_set_test.cpp
 *  @date 19.12.2014
 *  @author mwarsinsky
 *  @brief 
 *
 *  $LastChangedDate$
 *  $LastChangedBy$
 *  $LastChangedRevision$
 *
 */


#include <cppunit/extensions/HelperMacros.h>
#include <cppunit_toolbox/extensions/assert_equal_with_different_types.h>
#include "cpp_odbc/connection.h"
#include "pydbc/connection.h"
#include "mock_classes.h"
#include "pydbc/result_set.h"
#include <sqlext.h>
#include <boost/variant/get.hpp>
#include <cstring>


class result_set_test : public CppUnit::TestFixture {
CPPUNIT_TEST_SUITE( result_set_test );

	CPPUNIT_TEST( fetch_without_columns );
	CPPUNIT_TEST( fetch_with_single_string_column );
	CPPUNIT_TEST( fetch_with_single_integer_column );
	CPPUNIT_TEST( constructor_both_types );

CPPUNIT_TEST_SUITE_END();

public:

	void fetch_without_columns();
	void fetch_with_single_string_column();
	void fetch_with_single_integer_column();
	void constructor_both_types();

};

// Registers the fixture with the 'registry'
CPPUNIT_TEST_SUITE_REGISTRATION( result_set_test );

using pydbc_test::mock_connection;
using pydbc_test::mock_statement;

namespace {

	std::shared_ptr<testing::NiceMock<mock_statement>> prepare_mock_with_columns(std::vector<long> const & column_types)
	{
		auto statement = std::make_shared<testing::NiceMock<mock_statement>>();

		ON_CALL(*statement, do_number_of_columns())
			.WillByDefault(testing::Return(column_types.size()));

		for (std::size_t i = 0; i != column_types.size(); ++i) {
			ON_CALL(*statement, do_get_integer_column_attribute(i + 1, SQL_DESC_TYPE))
				.WillByDefault(testing::Return(column_types[i]));
		}
		return statement;
	}

	/**
	* Change the address of the given target_pointer to point to the third argument of the mocked function
	*/
	ACTION_P(store_pointer_to_buffer_in, target_pointer) {
		*target_pointer = &arg2;
	}

	/**
	 * Set the mock statement to expect calls to bind_buffer. Returns a vector of pointers to buffers
	 * which are bound to the individual columns. These values will be filled when bind_column is called.
	 */
	std::vector<cpp_odbc::multi_value_buffer *> expect_calls_to_bind_buffer(mock_statement & statement, std::vector<SQLSMALLINT> const & expected_bind_types)
	{
		std::vector<cpp_odbc::multi_value_buffer *> buffers(expected_bind_types.size(), nullptr);

		for (std::size_t i = 0; i != expected_bind_types.size(); ++i) {
			EXPECT_CALL(statement, do_bind_column(i + 1, expected_bind_types[0], testing::_))
				.WillOnce(store_pointer_to_buffer_in(&buffers[i]));
		}

		return buffers;
	}

}

void result_set_test::fetch_without_columns()
{
	auto statement = prepare_mock_with_columns({});

	ON_CALL(*statement, do_fetch_next())
		.WillByDefault(testing::Return(true));

	auto result_set = pydbc::result_set(statement);
	CPPUNIT_ASSERT_EQUAL(0, result_set.fetch_one().size());
}


namespace {
	/**
	* @brief Store the given string value as the first value in
	*        the buffer pointed to by pointer_to_buffer
	*/
	ACTION_P2(put_string_value_in_buffer, pointer_to_buffer, value) {
		auto element = (*pointer_to_buffer)[0];
		std::memcpy(element.data_pointer, value.data(), value.size() + 1);
		element.indicator = value.size();
	}
}


void result_set_test::fetch_with_single_string_column()
{
	auto statement = prepare_mock_with_columns({SQL_VARCHAR});
	auto buffers = expect_calls_to_bind_buffer(*statement, {SQL_CHAR});

	auto result_set = pydbc::result_set(statement);
	CPPUNIT_ASSERT(buffers[0] != nullptr);

	std::string const expected_value = "this is a test string";
	EXPECT_CALL(*statement, do_fetch_next())
		.WillOnce(testing::DoAll(
					put_string_value_in_buffer(buffers[0], expected_value),
					testing::Return(true)
				));

	auto row = result_set.fetch_one();
	CPPUNIT_ASSERT_EQUAL(1, row.size());
	CPPUNIT_ASSERT_EQUAL(expected_value, boost::get<std::string>(row[0]));
}


namespace {
	/**
	 * @brief Store the given binary value as the first value in
	 *        the buffer pointed to by pointer_to_buffer
	 */
	ACTION_P2(put_binary_value_in_buffer, pointer_to_buffer, value) {
		auto element = (*pointer_to_buffer)[0];
		std::memcpy(element.data_pointer, &value, sizeof(value));
		element.indicator = sizeof(value);
	}
}

void result_set_test::fetch_with_single_integer_column()
{
	auto statement = prepare_mock_with_columns({SQL_INTEGER});
	auto buffers = expect_calls_to_bind_buffer(*statement, {SQL_C_SBIGINT});

	auto result_set = pydbc::result_set(statement);
	CPPUNIT_ASSERT(buffers[0] != nullptr);

	long const expected_value = 42;
	EXPECT_CALL(*statement, do_fetch_next())
		.WillOnce(testing::DoAll(
					put_binary_value_in_buffer(buffers[0], expected_value),
					testing::Return(true)
				));

	auto row = result_set.fetch_one();
	CPPUNIT_ASSERT_EQUAL(1, row.size());
	CPPUNIT_ASSERT_EQUAL(expected_value, boost::get<long>(row[0]));
}


void result_set_test::constructor_both_types()
{
	auto statement = std::make_shared<testing::NiceMock<mock_statement>>();

	EXPECT_CALL(*statement, do_number_of_columns())
		.WillOnce(testing::Return(2));
	EXPECT_CALL(*statement, do_get_integer_column_attribute(1,SQL_DESC_TYPE))
		.WillOnce(testing::Return(SQL_VARCHAR));
	EXPECT_CALL(*statement, do_get_integer_column_attribute(2,SQL_DESC_TYPE))
		.WillOnce(testing::Return(SQL_INTEGER));
	EXPECT_CALL(*statement, do_bind_column(1, SQL_CHAR, testing::_)).Times(1);
	EXPECT_CALL(*statement, do_bind_column(2, SQL_C_SBIGINT, testing::_)).Times(1);

	auto result_set = pydbc::result_set(statement);
//	CPPUNIT_ASSERT_EQUAL(2, result_set.columns.size());
	CPPUNIT_FAIL("not implemented yet!");
}

