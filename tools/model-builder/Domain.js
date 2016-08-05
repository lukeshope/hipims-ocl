'use strict';

var DomainBNG = require('./DomainBNG');
var DomainLab = require('./DomainLab');

module.exports = {
	getDomainForType: function (domainType) {
		switch (domainType) {
			case 'world':
				return DomainBNG;
			case 'laboratory':
			case 'imaginary':
				return DomainLab;
		}
		
		return null;
	}
};
