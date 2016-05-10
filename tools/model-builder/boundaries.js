'use strict';

const fs = require('fs');

var boundaries = function(definition) {
	this.boundaryDefinition = definition;
};

boundaries.prototype.writeFiles = function(duration, directory, cb) {
	let fileCount = 2; 
	let fileFinished = 0;
	let fileComplete = function(err) {
		if (!err) {
			fileFinished++;
			if (fileFinished >= fileCount) {
				cb(false);
			}
		} else {
			cb(err);
		}
	};
	
	console.log('    Attempting to write rainfall intensity boundary.');
	fs.writeFile(
		directory + '/rainfall.csv',
		this.getRainfall(duration),
		fileComplete
	);
	
	console.log('    Attempting to write drainage boundary.');
	fs.writeFile(
		directory + '/drainage.csv',
		this.getDrainage(duration),
		fileComplete
	);
}

boundaries.prototype.getRainfall = function(duration) {
	let csvHeader = 'Time (s),Rainfall intensity (mm/hr)\n';
	let csvLines = '';
	for (let csvTime = 0.0; csvTime <= duration; csvTime += this.boundaryDefinition.rainfallDuration) {
		csvLines += csvTime + ',' + (csvTime < this.boundaryDefinition.rainfallDuration ? this.boundaryDefinition.rainfallIntensity : 0.0) + '\n';
	}
	return csvHeader + csvLines;
}


boundaries.prototype.getDrainage = function(duration) {
	let csvHeader = 'Time (s),Drainage rate (mm/hr)\n';
	let csvLines = '';
	csvLines += '0.0,' + this.boundaryDefinition.drainageRate + '\n';
	csvLines += duration + ',' + this.boundaryDefinition.drainageRate + '\n'
	return csvHeader + csvLines;
}

module.exports = boundaries;
