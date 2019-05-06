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

#pragma once

#include <test/libsolidity/AnalysisFramework.h>
#include <test/TestCase.h>
#include <liblangutil/Exceptions.h>
#include <libdevcore/AnsiColorized.h>

#include <iosfwd>
#include <string>
#include <vector>
#include <utility>

namespace dev
{
namespace solidity
{
namespace test
{

struct CreationCost
{
	u256 executionCost{0};
	u256 codeDepositCost{0};
	u256 totalCost{0};
};

class GasTest: AnalysisFramework, public EVMVersionRestrictedTestCase
{
public:
	static std::unique_ptr<TestCase> create(Config const& _config)
	{ return std::make_unique<GasTest>(_config.filename, _config.evmVersion); }
	GasTest(std::string const& _filename, langutil::EVMVersion _evmVersion);

	bool run(std::ostream& _stream, std::string const& _linePrefix = "", bool _formatted = false) override;

	void printSource(std::ostream &_stream, std::string const &_linePrefix = "", bool _formatted = false) const override;
	void printUpdatedExpectations(std::ostream& _stream, std::string const& _linePrefix) const override;

protected:

	void parseExpectations(std::istream& _stream);

	std::string m_source;
	CreationCost m_creationCost;
	std::map<std::string, std::string> m_externalFunctionCosts;
	std::map<std::string, std::string> m_internalFunctionCosts;
	langutil::EVMVersion const m_evmVersion;
};

}
}
}
