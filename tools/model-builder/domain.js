'use strict';

var domainBNG = require('./domainBNG');
var domainLab = require('./domainLab');

module.exports = {
	getDomainForType: function (domainType) {
		switch (domainType) {
			case 'world':
				return domainBNG;
			case 'laboratory':
			case 'imaginary':
				return domainLab;
		}
		
		return null;
	}
};
