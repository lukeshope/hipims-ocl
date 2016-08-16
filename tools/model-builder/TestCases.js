'use strict';

var testDefinitions = {
	'SLOSHING PARABOLIC BOWL':	require('./tests/TestSloshingBowl'),
	'LAKE AT REST':	require('./tests/TestLakeAtRest'),
	'DAM BREAK OVER AN EMERGING BED': require('./tests/TestDamBreakEmergingBed'),
	'DAM BREAK AGAINST AN OBSTACLE': require('./tests/TestDamBreakAgainstObstacle')
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
