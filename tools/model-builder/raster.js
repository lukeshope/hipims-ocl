'use strict';

var gdal = require('gdal');

var rasterTools = function() {
	// ...
};

var thisInstance = null;
module.exports = function () {
	if ( !thisInstance ) {
		thisInstance = new rasterTools();
	}
	return thisInstance;
}();
