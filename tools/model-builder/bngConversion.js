'use strict';

var bngConversion = function() {
	// ...
};

bngConversion.prototype.enToRef = function(easting, northing, precision) {
	const gridChars = 'ABCDEFGHJKLMNOPQRSTUVWXYZ';
	var letterCoordE = Math.floor(easting / 100000);
	var letterCoordN = Math.floor(northing / 100000);
	if (letterCoordE > 6 || 
	    letterCoordE < 0 || 
		letterCoordN < 0 ||
		letterCoordN > 12) {
		return '';
	}
	var letters = gridChars.charAt(
					(19 - letterCoordN) 
					- ((19 - letterCoordN) % 5) 
					+ Math.floor((letterCoordE + 10) / 5)
				  ) + gridChars.charAt(
				    (19 - letterCoordN) * 5 % 25
					+ letterCoordE % 5
				  );
	var subLetterCoordE = easting % 100000;
	var subLetterCoordN = northing % 100000;
	subLetterCoordE = ('00000'.substring(0, Math.max(0, 5 - subLetterCoordE.toString().length)) + subLetterCoordE.toString()).substring(0, precision);
	subLetterCoordN = ('00000'.substring(0, Math.max(0, 5 - subLetterCoordN.toString().length)) + subLetterCoordN.toString()).substring(0, precision);
	return letters + subLetterCoordE + subLetterCoordN;
};

var thisInstance = null;
module.exports = function () {
	if ( !thisInstance ) {
		thisInstance = new bngConversion();
	}
	return thisInstance;
}();
