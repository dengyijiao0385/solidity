/*
    This file is part of solidity.

    solidity is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    solidity is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with solidity.  If not, see <http://www.gnu.org/licenses/>.
*/
/**
 * @author Christian <c@ethdev.com>
 * @date 2016
 * Unit tests for inline assembly.
 */

#include <string>
#include <memory>
#include <libevmasm/Assembly.h>
#include <libsolidity/parsing/Scanner.h>
#include <libsolidity/inlineasm/AsmStack.h>
#include <libsolidity/interface/Exceptions.h>
#include <libsolidity/ast/AST.h>
#include "../TestHelper.h"

using namespace std;

namespace dev
{
namespace solidity
{
namespace test
{

namespace
{

bool successParse(std::string const& _source, bool _assemble = false, bool _allowWarnings = true)
{
	assembly::InlineAssemblyStack stack;
	try
	{
		if (!stack.parse(std::make_shared<Scanner>(CharStream(_source))))
			return false;
		if (_assemble)
		{
			stack.assemble();
			if (!stack.errors().empty())
				if (!_allowWarnings || !Error::containsOnlyWarnings(stack.errors()))
					return false;
		}
	}
	catch (FatalError const&)
	{
		if (Error::containsErrorOfType(stack.errors(), Error::Type::ParserError))
			return false;
	}
	if (Error::containsErrorOfType(stack.errors(), Error::Type::ParserError))
		return false;

	BOOST_CHECK(Error::containsOnlyWarnings(stack.errors()));
	return true;
}

bool successAssemble(string const& _source, bool _allowWarnings = true)
{
	return successParse(_source, true, _allowWarnings);
}

void parsePrintCompare(string const& _source)
{
	assembly::InlineAssemblyStack stack;
	BOOST_REQUIRE(stack.parse(std::make_shared<Scanner>(CharStream(_source))));
	BOOST_REQUIRE(stack.errors().empty());
	BOOST_CHECK_EQUAL(stack.toString(), _source);
}

}


BOOST_AUTO_TEST_SUITE(SolidityInlineAssembly)


BOOST_AUTO_TEST_SUITE(Parsing)

BOOST_AUTO_TEST_CASE(smoke_test)
{
	BOOST_CHECK(successParse("{ }"));
}

BOOST_AUTO_TEST_CASE(simple_instructions)
{
	BOOST_CHECK(successParse("{ dup1 dup1 mul dup1 sub }"));
}

BOOST_AUTO_TEST_CASE(suicide_selfdestruct)
{
	BOOST_CHECK(successParse("{ suicide selfdestruct }"));
}

BOOST_AUTO_TEST_CASE(keywords)
{
	BOOST_CHECK(successParse("{ byte return address }"));
}

BOOST_AUTO_TEST_CASE(constants)
{
	BOOST_CHECK(successParse("{ 7 8 mul }"));
}

BOOST_AUTO_TEST_CASE(vardecl)
{
	BOOST_CHECK(successParse("{ let x := 7 }"));
}

BOOST_AUTO_TEST_CASE(assignment)
{
	BOOST_CHECK(successParse("{ 7 8 add =: x }"));
}

BOOST_AUTO_TEST_CASE(label)
{
	BOOST_CHECK(successParse("{ 7 abc: 8 eq abc jump }"));
}

BOOST_AUTO_TEST_CASE(label_complex)
{
	BOOST_CHECK(successParse("{ 7 abc: 8 eq jump(abc) jumpi(eq(7, 8), abc) }"));
}

BOOST_AUTO_TEST_CASE(functional)
{
	BOOST_CHECK(successParse("{ add(7, mul(6, x)) add mul(7, 8) }"));
}

BOOST_AUTO_TEST_CASE(functional_assignment)
{
	BOOST_CHECK(successParse("{ x := 7 }"));
}

BOOST_AUTO_TEST_CASE(functional_assignment_complex)
{
	BOOST_CHECK(successParse("{ x := add(7, mul(6, x)) add mul(7, 8) }"));
}

BOOST_AUTO_TEST_CASE(vardecl_complex)
{
	BOOST_CHECK(successParse("{ let x := add(7, mul(6, x)) add mul(7, 8) }"));
}

BOOST_AUTO_TEST_CASE(blocks)
{
	BOOST_CHECK(successParse("{ let x := 7 { let y := 3 } { let z := 2 } }"));
}

BOOST_AUTO_TEST_CASE(labels_with_stack_info)
{
	BOOST_CHECK(successParse("{ x[-1]: y[a]: z[d, e]: h[100]: g[]: }"));
}

BOOST_AUTO_TEST_CASE(function_definitions)
{
	BOOST_CHECK(successParse("{ function f() { } function g(a) -> (x) { } }"));
}

BOOST_AUTO_TEST_CASE(function_definitions_multiple_args)
{
	BOOST_CHECK(successParse("{ function f(a, d) { } function g(a, d) -> (x, y) { } }"));
}

BOOST_AUTO_TEST_CASE(function_calls)
{
	BOOST_CHECK(successParse("{ g(1, 2, f(mul(2, 3))) x() }"));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(Printing)

BOOST_AUTO_TEST_CASE(print_smoke)
{
	parsePrintCompare("{\n}");
}

BOOST_AUTO_TEST_CASE(print_instructions)
{
	parsePrintCompare("{\n    7\n    8\n    mul\n    dup10\n    add\n}");
}

BOOST_AUTO_TEST_CASE(print_subblock)
{
	parsePrintCompare("{\n    {\n        dup4\n        add\n    }\n}");
}

BOOST_AUTO_TEST_CASE(print_functional)
{
	parsePrintCompare("{\n    mul(sload(0x12), 7)\n}");
}

BOOST_AUTO_TEST_CASE(print_label)
{
	parsePrintCompare("{\n    loop:\n    jump(loop)\n}");
}

BOOST_AUTO_TEST_CASE(print_label_with_stack)
{
	parsePrintCompare("{\n    loop[x, y]:\n    other[-2]:\n    third[10]:\n}");
}

BOOST_AUTO_TEST_CASE(print_assignments)
{
	parsePrintCompare("{\n    let x := mul(2, 3)\n    7\n    =: x\n    x := add(1, 2)\n}");
}

BOOST_AUTO_TEST_CASE(print_string_literals)
{
	parsePrintCompare("{\n    \"\\n'\\xab\\x95\\\"\"\n}");
}

BOOST_AUTO_TEST_CASE(print_string_literal_unicode)
{
	string source = "{ \"\\u1bac\" }";
	string parsed = "{\n    \"\\xe1\\xae\\xac\"\n}";
	assembly::InlineAssemblyStack stack;
	BOOST_REQUIRE(stack.parse(std::make_shared<Scanner>(CharStream(source))));
	BOOST_REQUIRE(stack.errors().empty());
	BOOST_CHECK_EQUAL(stack.toString(), parsed);
	parsePrintCompare(parsed);
}

BOOST_AUTO_TEST_CASE(function_definitions_multiple_args)
{
	parsePrintCompare("{\n    function f(a, d)\n    {\n        mstore(a, d)\n    }\n    function g(a, d) -> (x, y)\n    {\n    }\n}");
}

BOOST_AUTO_TEST_CASE(function_calls)
{
	parsePrintCompare("{\n    g(1, mul(2, x), f(mul(2, 3)))\n    x()\n}");
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE(Analysis)

BOOST_AUTO_TEST_CASE(string_literals)
{
	BOOST_CHECK(successAssemble("{ let x := \"12345678901234567890123456789012\" }"));
}

BOOST_AUTO_TEST_CASE(oversize_string_literals)
{
	BOOST_CHECK(!successAssemble("{ let x := \"123456789012345678901234567890123\" }"));
}

BOOST_AUTO_TEST_CASE(assignment_after_tag)
{
	BOOST_CHECK(successParse("{ let x := 1 { tag: =: x } }"));
}

BOOST_AUTO_TEST_CASE(magic_variables)
{
	BOOST_CHECK(!successAssemble("{ this }"));
	BOOST_CHECK(!successAssemble("{ ecrecover }"));
	BOOST_CHECK(successAssemble("{ let ecrecover := 1 ecrecover }"));
}

BOOST_AUTO_TEST_CASE(imbalanced_stack)
{
	BOOST_CHECK(successAssemble("{ 1 2 mul pop }", false));
	BOOST_CHECK(!successAssemble("{ 1 }", false));
	BOOST_CHECK(successAssemble("{ let x := 4 7 add }", false));
}

BOOST_AUTO_TEST_CASE(error_tag)
{
	BOOST_CHECK(successAssemble("{ invalidJumpLabel }"));
}

BOOST_AUTO_TEST_CASE(designated_invalid_instruction)
{
	BOOST_CHECK(successAssemble("{ invalid }"));
}

BOOST_AUTO_TEST_CASE(inline_assembly_shadowed_instruction_declaration)
{
	// Error message: "Cannot use instruction names for identifier names."
	BOOST_CHECK(!successAssemble("{ let gas := 1 }"));
}

BOOST_AUTO_TEST_CASE(inline_assembly_shadowed_instruction_assignment)
{
	// Error message: "Identifier expected, got instruction name."
	BOOST_CHECK(!successAssemble("{ 2 =: gas }"));
}

BOOST_AUTO_TEST_CASE(inline_assembly_shadowed_instruction_functional_assignment)
{
	// Error message: "Cannot use instruction names for identifier names."
	BOOST_CHECK(!successAssemble("{ gas := 2 }"));
}

BOOST_AUTO_TEST_CASE(revert)
{
	BOOST_CHECK(successAssemble("{ revert(0, 0) }"));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_AUTO_TEST_SUITE_END()

}
}
} // end namespaces
