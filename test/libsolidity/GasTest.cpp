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

#include <test/libsolidity/GasTest.h>
#include <test/Options.h>
#include <libdevcore/CommonIO.h>
#include <libdevcore/JSON.h>
#include <boost/algorithm/string.hpp>
#include <boost/algorithm/string/predicate.hpp>
#include <boost/throw_exception.hpp>
#include <fstream>
#include <memory>
#include <stdexcept>

using namespace langutil;
using namespace dev::solidity;
using namespace dev::solidity::test;
using namespace dev::formatting;
using namespace dev;
using namespace std;
namespace fs = boost::filesystem;
using namespace boost::unit_test;


namespace
{

// TODO: shared with SyntaxTest.cpp
int parseUnsignedInteger(string::iterator& _it, string::iterator _end)
{
	if (_it == _end || !isdigit(*_it))
		throw runtime_error("Invalid test expectation. Source location expected.");
	int result = 0;
	while (_it != _end && isdigit(*_it))
	{
		result *= 10;
		result += *_it - '0';
		++_it;
	}
	return result;
}

}

GasTest::GasTest(string const& _filename, langutil::EVMVersion _evmVersion): m_evmVersion(_evmVersion)
{
	ifstream file(_filename);
	if (!file)
		BOOST_THROW_EXCEPTION(runtime_error("Cannot open test contract: \"" + _filename + "\"."));
	file.exceptions(ios::badbit);

	m_source = parseSourceAndSettings(file);
	parseExpectations(file);
}

void GasTest::parseExpectations(std::istream& _stream)
{
	bool gotCreation = false;
	std::string line;

	std::map<std::string, std::string>* currentKind = nullptr;

	std::stringstream expectations;

	while (getline(_stream, line))
	{
		if (line.find("// creation:") == 0)
		{
			auto it = line.begin() + 12;
			skipWhitespace(it, line.end());
			m_creationCost.executionCost = parseUnsignedInteger(it, line.end());
			skipWhitespace(it, line.end());
			if (*it++ != '+')
				BOOST_THROW_EXCEPTION(runtime_error("Invalid expectation: expected \"+\"-"));
			skipWhitespace(it, line.end());
			m_creationCost.codeDepositCost = parseUnsignedInteger(it, line.end());
			skipWhitespace(it, line.end());
			if (*it++ != '=')
				BOOST_THROW_EXCEPTION(runtime_error("Invalid expectation: expected \"+\"-"));
			skipWhitespace(it, line.end());
			m_creationCost.totalCost = parseUnsignedInteger(it, line.end());
			gotCreation = true;
		} else if (line == "// external:")
			currentKind = &m_externalFunctionCosts;
		else if (line == "// internal:")
			currentKind = &m_internalFunctionCosts;
		else if (!currentKind)
			BOOST_THROW_EXCEPTION(runtime_error("No function kind specified. Expected \"external:\" or \"internal:\"."));
		else
		{
			if (!boost::starts_with(line, "// "))
				BOOST_THROW_EXCEPTION(runtime_error("Invalid expectation: expected \"// \"-"));
			auto it = line.begin() + 3;
			expect(it, line.end(), ' ');
			expect(it, line.end(), ' ');
			auto functionBegin = it;
			while (it != line.end() && *it != ':')
				++it;
			auto& entry = (*currentKind)[std::string(functionBegin, it)];
			expect(it, line.end(), ':');
			skipWhitespace(it, line.end());
			if (it == line.end())
				BOOST_THROW_EXCEPTION(runtime_error("Invalid expectation: expected gas cost."));
			entry = std::string(it, line.end());
		}
	}
	if (!gotCreation)
		BOOST_THROW_EXCEPTION(runtime_error("No creation costs specified."));
}

void GasTest::printUpdatedExpectations(std::ostream& _stream, std::string const& _linePrefix) const
{
	Json::Value estimates = compiler().gasEstimates(compiler().lastContractName());
	_stream << _linePrefix << "creation: " << estimates["creation"]["executionCost"].asString() << " + "
			<< estimates["creation"]["codeDepositCost"].asString() << " = "
			<< estimates["creation"]["totalCost"].asString() << std::endl;

	for (auto kind: {"external", "internal"})
	{
		if (estimates[kind]) {
			_stream << _linePrefix << kind << ":" << std::endl;
			for (auto it = estimates[kind].begin(); it != estimates[kind].end(); ++it)
			{
				_stream << _linePrefix << "  " << it.key().asString() << ": " << it->asString() << std::endl;
			}
		}
	}
}


bool GasTest::run(ostream& _stream, string const& _linePrefix, bool /* _formatted */)
{
	string const versionPragma = "pragma solidity >=0.0;\n";
	compiler().reset();
	compiler().setSources({{"", versionPragma + m_source}});
	compiler().setEVMVersion(m_evmVersion);

	if (compiler().parse())
		if (compiler().analyze())
			compiler().compile();

	Json::Value estimates = compiler().gasEstimates(compiler().lastContractName());

	bool success = true;

	{
		auto creation = estimates["creation"];
		success &= (creation["codeDepositCost"].asString() == toString(m_creationCost.codeDepositCost));
		success &= (creation["executionCost"].asString() == toString(m_creationCost.executionCost));
		success &= (creation["totalCost"].asString() == toString(m_creationCost.totalCost));
	}

	auto checkFunctions = [&](map<string, string> const& _a, Json::Value const& _b) {
		for (auto& entry: _a) {
			success &= _b[entry.first].asString() == entry.second;
		}
	};
	checkFunctions(m_internalFunctionCosts, estimates["internal"]);
	checkFunctions(m_externalFunctionCosts, estimates["external"]);

	_stream << _linePrefix << "Mismatching cost:" << std::endl;
	_stream << _linePrefix << "Expected:" << std::endl;
	_stream << _linePrefix << "  creation: " << toString(m_creationCost.executionCost) << " + "
			<< toString(m_creationCost.codeDepositCost) << " = " << toString(m_creationCost.totalCost) << std::endl;
	auto printExpected = [&](std::string const& _kind, std::map<std::string, std::string> const& _expectations) {
		_stream << _linePrefix << "  " << _kind << ":" << std::endl;
		for (auto const& entry: _expectations) {
			_stream << _linePrefix << "    " << entry.first << ": " << entry.second << std::endl;
		}
	};
	if (!m_externalFunctionCosts.empty())
		printExpected("external", m_externalFunctionCosts);
	if (!m_internalFunctionCosts.empty())
		printExpected("internal", m_internalFunctionCosts);
	_stream << _linePrefix << "Obtained:" << std::endl;
	printUpdatedExpectations(_stream, _linePrefix + "  ");

	return success;
}

void GasTest::printSource(ostream& _stream, string const& _linePrefix, bool) const
{
	std::string line;
	std::istringstream input(m_source);
	while (getline(input, line))
		_stream << _linePrefix << line << std::endl;
}
