/* 31735 */

'use strict'

// C library API
const ffi = require('ffi');
// Express App (Routes)
const express = require("express");
const app     = express();
const path    = require("path");
const fileUpload = require('express-fileupload');
const bodyParser = require('body-parser');
const mysql = require('mysql');
var connection;

app.use(fileUpload());
app.use(bodyParser.urlencoded({
  extended: true
}));

// Minimization
const fs = require('fs');
const ref = require('ref');
const JavaScriptObfuscator = require('javascript-obfuscator');

// Important, pass in port as in `npm run dev 1234`, do not change
const portNum = process.argv[2];

let ICalErrorCode = ref.types.int;
let ICalErrorCodePtr = ref.refType(ICalErrorCode);
let Calendar = ref.types.void;
let CalendarPtr = ref.refType(Calendar);
let CalendarPtrPtr = ref.refType(CalendarPtr);

//Create the mapping from C
let parserLib = ffi.Library("./libcal.so", {
  "createCalendar" : [ICalErrorCode, ["string", CalendarPtrPtr] ],
  "calendarToJSON" : ["string", [CalendarPtr]],
  "printError" : ["string", [ICalErrorCode]],
  "validateCalendar" : [ICalErrorCode, [CalendarPtr]],
  "eventListToJSONWrapper" : ["string", [CalendarPtr]],
  "alarmListToJSONWrapper" : ["string", [CalendarPtr, ref.types.int]],
  "deleteCalendar" : [ref.types.void, [CalendarPtr]],
  "JSONtoEventWrapper": [ref.types.void, [CalendarPtr, "string"]],
  "writeCalendar" : [ICalErrorCode, ["string", CalendarPtr]]
});

function createFileTable() {
  let query = 'CREATE TABLE IF NOT EXISTS FILE ('
  query += 'cal_id INT AUTO_INCREMENT PRIMARY KEY, '; 
  query += 'file_Name VARCHAR(60) NOT NULL, ';
  query += 'version INT NOT NULL, ';
  query += 'prod_id VARCHAR(256) NOT NULL );';
  if (connection) {
    connection.query(query);
  }
}

function find_prop(propArray, tofind) {
  for (let i = 0; i < propArray.length; i++) {
    if (propArray[i].propName.toLowerCase() === tofind.toLowerCase()) {
      return propArray[i].propDescr;
    }
  }
  return undefined;
}

function insert_file(filename, data, eventsPrior) {
  
  console.log(data);

  if (connection) {
    let found = false;
    connection.query( 'SELECT * FROM FILE WHERE file_Name = "' + filename + '"', (error, result) => {
      if (error) {
        console.log('Error occured while trying to check if ' + filename + ' exists!');
        return;
      } else {
        found = (result.length > 0);
        //so if we don't already have this record
        if (!found) {
          //insert the file thing
          let qr = 'INSERT INTO FILE (file_Name, version, prod_id) VALUES (\'' + filename + '\', ' + data.version + ', \'' + data.prodID + '\');';
          connection.query(qr, (error, result) => {
            if (error) {
              console.log(error);
              return;
            } else {
              
              let no_errors = true;
              
              for (let i = 0; i < data.numEvents; i++) {
                
                let numAlarms = data.eventList[i].alarms.length;
                let summary = find_prop(data.eventList[i].props, 'summary') ? '"' + find_prop(data.eventList[i].props, 'summary') + '"' : 'NULL';
                let start_time = '"' + data.eventList[i].startDT.date + data.eventList[i].startDT.time + '"';
                let location = find_prop(data.eventList[i].props, 'location') ? '"' + find_prop(data.eventList[i].props, 'location') + '"' : 'NULL';
                let organizer = find_prop(data.eventList[i].props, 'organizer') ? '"' + find_prop(data.eventList[i].props, 'organizer') + '"' : 'NULL';
                
                connection.query('INSERT INTO EVENT SET summary = ' + summary + ', start_time = ' + start_time + ', location = ' + location + ', organizer = ' + organizer + ', cal_file = (SELECT cal_id FROM FILE WHERE file_Name = "' + filename + '");', (error2, result2) => {
                  
                  if (error2) {
                    console.log(error2);
                    return;
                  }

                  console.log('Event #' + (eventsPrior + i + 1) + " has " + numAlarms + ' alarms with summary: ' + summary)

                  for (let j = 0; j < numAlarms; j++) {
                    
                    connection.query('INSERT INTO ALARM SET action = "' + data.eventList[i].alarms[j].action + '", `trigger`= "' + data.eventList[i].alarms[j].trigger + '", event = ' + (eventsPrior + i + 1) + ';', (error3, result3) => {

                      if (error3) {
                        console.log(error3);
                        return;
                      }
                    });
                  }

                });
              }
            }
          });
        }
      }
    });
  }
}


function createEventTable() {
  let query = 'CREATE TABLE IF NOT EXISTS EVENT (';
  query += 'event_id INT AUTO_INCREMENT PRIMARY KEY, ';
  query += 'summary VARCHAR(1024), ';
  query += 'start_time DATETIME NOT NULL, ';
  query += 'location VARCHAR(60), ';
  query += 'organizer VARCHAR(256), ';
  query += 'cal_file INT NOT NULL, ';
  query += 'FOREIGN KEY (cal_file) REFERENCES FILE(cal_id) ON DELETE CASCADE);';
  if (connection) {
    connection.query(query);
  }
}

function createAlarmTable() {
  let query = 'CREATE TABLE IF NOT EXISTS ALARM (';
  query += 'alarm_id INT AUTO_INCREMENT PRIMARY KEY, ';
  query += 'action VARCHAR(256) NOT NULL, ';
  query += '`trigger` VARCHAR(256) NOT NULL, ';
  query += 'event INT NOT NULL, ';
  query += 'FOREIGN KEY (event) REFERENCES EVENT(event_id) ON DELETE CASCADE);';
  if (connection) {
    connection.query(query, function(err, stat) {
      /*
      if (!err) {
        insert_file('coolfile', {
          version : 2,
          prodId : 'super yeet',
          eventList : [
            {
              summary : 'first event',
              start_time : '2038-01-19 03:14:07',
              location : 'Canada',
              organizer : 'john doe',
              cal_file : 1,
              alarms : [
                {
                  action : 'AUDIO',
                  trigger : 'swag',
                  event : 1
                }
              ]
            }
          ]
        });
      } */
    });
  }
}

function getCalendar(filename) {
  let calendar = ref.alloc(CalendarPtrPtr);
  let name = "./uploads/" + filename;
  let obj = parserLib.createCalendar(name, calendar);
  /* Validate that the Calendar parsed okay */
  if (getError(filename) === 'OK') {
    var retObj = JSON.parse(parserLib.calendarToJSON(calendar.deref()));
    retObj['eventList'] = JSON.parse(parserLib.eventListToJSONWrapper(calendar.deref()));
    for (var i = 0; i < retObj['eventList'].length; i++) {
      retObj['eventList'][i]['alarms'] = JSON.parse(parserLib.alarmListToJSONWrapper(calendar.deref(), i + 1));
    }
    parserLib.deleteCalendar(calendar.deref());
    return retObj;
  } else {
    /* Else return a NULL */
    return null;
  }
}

function addEventToCalendar(json) {
  let calendar = ref.alloc(CalendarPtrPtr);
  let name = "./uploads/" + json['filename'] + '.ics';
  let obj = parserLib.createCalendar(name, calendar);

  let filename = json['filename'] + '.ics';

  if (getError(filename) === 'OK') {
    delete json['filename'];
    parserLib.JSONtoEventWrapper(calendar.deref(), JSON.stringify(json));
    let writeStatus = parserLib.writeCalendar('uploads/' + filename, calendar.deref());
    parserLib.deleteCalendar(calendar.deref());
    /* Check */
    if (parserLib.printError(writeStatus) === 'OK') {
      return 1;
    } else {
      return 0;
    }
  }
}

function getError(filename) {
  let calendar = ref.alloc(CalendarPtrPtr);
  let name = "./uploads/" + filename;
  let obj = parserLib.createCalendar(name, calendar);

  let errorCode = parserLib.printError(obj);

  if (errorCode === 'OK') {
    errorCode = parserLib.printError(parserLib.validateCalendar(calendar.deref()));
    if (errorCode === 'OK') {
      return 'OK';
    } else {
      return errorCode;
    }
  } else {
    return errorCode;
  }
}

// Send HTML at root, do not change
app.get('/',function(req,res){
  res.sendFile(path.join(__dirname+'/public/index.html'));
});

// Send obfuscated JS, do not change
app.get('/index.js',function(req,res){
  fs.readFile(path.join(__dirname + '/public/index.js'), 'utf8', function(err, contents) {
    const minimizedContents = JavaScriptObfuscator.obfuscate(contents, {compact: true, controlFlowFlattening: true});
    res.contentType('application/javascript');
    res.send(minimizedContents._obfuscatedCode);
  });
});

app.post('/addevent', function(req, res) {
  let json = req.body;
  if (addEventToCalendar(req.body) === 1) {
    res.status(200).send('Success!');
  } else {
    res.status(400).send('Could not write to file!');
  }
});

app.get('/dbstatus', function(req, res) {
  connection.query('select (select count(*) from FILE) as fc, (select count(*) from EVENT) as ec, (select count(*) from ALARM) as ac;', function(err, result) {
    if (err) res.status(400).send('Could not retrieve db status');
    else {
      res.send( {'FILE' : result[0]['fc'], 'EVENT' : result[0]['ec'], 'ALARM' : result[0]['ac']});
    }
  });
});

app.post('/create', function(req, res) {
  
  let json = req.body;

  var content = 'BEGIN:VCALENDAR\r\n';

  /* Basic shit */
  content += 'VERSION:' + json['version'] + '\r\n';
  content += 'PRODID:' + json['prodId'] + '\r\n';

  /* Event */
  let numEvents = parseInt(json['numEvents']);
  for (var i = 0; i < numEvents; i++) {
    content += 'BEGIN:VEVENT\r\n';

    content += 'UID:' + json['events'][i]['uid'] + '\r\n';
    content += 'DTSTAMP:' + json['events'][i]['dtstamp'] + '\r\n'; 
    content += 'DTSTART:' + json['events'][i]['dtstart'] + '\r\n';

    let numProps = parseInt(json['events'][i]['numProps']);
    for (let j = 0; j < numProps; j++) {
      content += json['events'][i]['props'][j]['propName'] + ':' + json['events'][i]['props'][j]['propDescr'] + '\r\n'; 
    }

    content += 'END:VEVENT\r\n';
  }
  content += 'END:VCALENDAR\r\n';

  fs.writeFile(path.join(__dirname + '/uploads/' + json['name'] + '.ics'), content, function(err) {
    
    if (err) {
      return res.status(400).send('Could not create file on the server!');
    }

    let createAttempt = getError(json['name'] + '.ics');

    if ( createAttempt !== 'OK') {
      fs.unlink(path.join(__dirname + '/uploads/' + json['name'] + '.ics'), function() { console.log('Attempted delete ' + json['name'])});
      return res.status(400).send(createAttempt);
    } else {
      return res.status(200).send('Nice bruv');
    }
  });

});

app.post('/query', function(req, res) {
  let qnum = req.body.qnum;
  let table = {
    '1': 'SELECT * FROM EVENT ORDER BY UNIX_TIMESTAMP(start_time) DESC;',
    '2': 'SELECT event_id, summary, start_time FROM EVENT WHERE cal_file = (SELECT cal_id FROM FILE WHERE file_Name = "' + req.body.filename + '" );',
    '3': 'SELECT * FROM EVENT WHERE start_time IN (SELECT start_time FROM EVENT GROUP BY start_time HAVING COUNT(*) > 1) ORDER BY UNIX_TIMESTAMP(start_time) DESC;',
    '4': 'SELECT * FROM EVENT et WHERE EXISTS (SELECT event FROM ALARM at WHERE at.event = et.event_id);',
    '5': 'SELECT file_Name, action, `trigger`, alarm_id FROM ALARM INNER JOIN EVENT ON event = event_id INNER JOIN FILE ON cal_file = cal_id;',
    '6': 'SELECT * FROM EVENT WHERE organizer = "' + req.body.organizer + '";'
  }
  connection.query(table[new String(qnum)], (err, response) => {
    if (err) {
      console.log(err);
      res.status(400).send('There occured an error on the server.');
    }
    if (response) {
      if (qnum === '5') for (let i = 0; i < response.length; i++) response[i]['isAlarm'] = true;
      
      res.send(response);
    }
  });
});

app.get('/deleteallfiles', function(req, res) {
  connection.query('DELETE FROM ALARM; DELETE FROM EVENT; DELETE FROM FILE; ALTER TABLE ALARM AUTO_INCREMENT = 1; ALTER TABLE EVENT AUTO_INCREMENT = 1; ALTER TABLE FILE AUTO_INCREMENT = 1;', [1,2,3,4,5,6], function(err, results) {
    if (err) {
      res.status(400).send('Unable to clear table!');
    }
    res.send('Success!');
  });
});

app.get('/saveallfiles', function(req, res) {
  fs.readdir('./uploads', (err, files) => {
    let prior = 0;
    files.forEach( file => {
        var json = getCalendar(file);
        if (json !== null) {
          insert_file(file, json, prior);
          prior += json.numEvents;
        }
    });
    res.send('Success!');
  });
});

//Respond to POST requests that uploads files to uploads/ directory
app.post('/uploads', function(req, res) {
  if(!req.files) {
    return res.status(400).send('No files were uploaded.');
  }

  let uploadFile = req.files.uploadFile;

// Use the mv() method to place the file somewhere on your server
  uploadFile.mv(path.join(__dirname + '/uploads/' + uploadFile.name), function(err) {
    /* Test the file */
    let errCode = getError(uploadFile.name);
    if ( err ||  errCode !== 'OK') {
      fs.unlink(path.join(__dirname + '/uploads/' + uploadFile.name), function() {
        console.log('Attempted to delete ' + uploadFile.name);
      });
      return res.status(500).send(errCode);
    }

    res.redirect('/');
  });

});

//Respond to GET requests for files in the uploads/ directory
app.get('/uploads/:name', function(req , res){
  fs.stat(path.join(__dirname + '/uploads/' + req.params.name), function(err, stat) {
    console.log(err);
    if(err == null) {
      res.sendFile(path.join(__dirname+'/uploads/' + req.params.name));
    } else {
      res.send('');
    }
  });
});

//******************** Your code goes here ********************

//Sample endpoint

app.get('/getnumfiles', function(req, res) {
  fs.readdir('./uploads', (err, files) => {
    var calendars = [];
    let size = 0;
    files.forEach( file => {
        var json = getCalendar(file);
        if (json !== null) {
          size++;
          json['filename'] = file;
          calendars.push(json);
        }
    });
    res.send({
      numFiles: size,
      files: calendars
    });
  });
});

app.get('/getfile/:filename', function(req, res) {
  fs.readdir('./uploads', (err, files) => {
    var to_send = null;
    files.forEach( file => {
        var json = getCalendar(file);
        if (json !== null && json['filename'] === req.params.filename) {
          to_send = json;
        }
    });
    res.send({
      file: to_send
    });
  });
});

app.listen(portNum);
console.log('Running app at localhost: ' + portNum);

