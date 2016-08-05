'use strict';

var testDefinitions = {
	'Sloshing Parabolic Bowl':	require('./tests/TestSloshingBowl')
};

module.exports = {
	getTestByName: function (testName) {
		if (testDefinitions[testName]) {
			return testDefinitions[testName].getTest();
		}

		return null;
	}
};
