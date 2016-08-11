'use strict';

var testDefinitions = {
	'SLOSHING PARABOLIC BOWL':	require('./tests/TestSloshingBowl'),
	'LAKE AT REST':	require('./tests/TestLakeAtRest')
};

module.exports = {
	getTestByName: function (testName) {
		testName = testName.toUpperCase();
		if (testDefinitions[testName]) {
			return testDefinitions[testName];
		}

		return null;
	}
};
