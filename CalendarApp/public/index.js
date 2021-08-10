var fileCount = 1;
var calendars = {};
let connected = false;

jQuery.fn.scrollTo = function(elem, speed) { 
  $(this).animate({
      scrollTop:  $(this).scrollTop() - $(this).offset().top + $(elem).offset().top 
  }, speed == undefined ? 1000 : speed); 
  return this; 
};

$(document).ready(function() {
  fileLog(true);
  $('select').formSelect();
  $('.datepicker').datepicker();
  $('.timepicker').timepicker();
  $('.dropdown-trigger').dropdown();
  // $('#query_textarea').css({
  //   'font-family' : 'Consolas'
  // });

  /* This is where we will re-render the calendar */
  $('#caldropdown').on('change', function() {
    $('#calview').empty();
    let caltable = '<table class="responsive-table"><thead><th>Event No</th><th>Start Date</th><th>Start Time</th><th>Summary</th><th>Props</th><th>Alarms</th></thead>';
    caltable += '<tbody>';
    for (var i = 0; i < calendars[$(this).val()].length; i++) {
      var row = calendars[$(this).val()][i];
      console.log(row);
      caltable += '<tr class="grey lighten-5"><td>' + (1 + i) + '</td><td>' + formatDate(row['startDT']['date']) + '</td><td>' + formatTime(row['startDT']['time']) + (row['startDT']['isUTC'] ? ' (UTC)' : '') + '</td><td>' + row['summary'] + '</td><td><a style="cursor: pointer" onclick="$(\'.properties_' + (i+1) + '\').toggle()">' + row['numProps'] + '</a></td><td><a style="cursor: pointer" onclick="$(\'.alarms_' + (i+1) + '\').toggle()">' + row['numAlarms'] + '</a></td></tr>';
      caltable += '<tr style="display:none" class="grey lighten-1 properties_' + (i + 1) + '"><th></th><th>Prop Name</th><th>Prop Value</th><th></th><th></th><th></th></tr>';
      caltable += '<tr style="display: none" class="properties_' + (i+1) + '"><td></td><td>UID</td><td>' + row['UID']  + '</td><td></td><td></td></tr>';
      caltable += '<tr style="display: none" class="properties_' + (i+1) + '"><td></td><td>Creation Date & Time</td><td>' + formatDate(row['stampDate']['date']) + ' at ' + formatTime(row['stampDate']['time']) + (row['stampDate']['UTC'] ? ' (UTC)' : '') + '</td><td></td><td></td></tr>';
      caltable += '<tr style="display: none" class="properties_' + (i+1) + '"><td></td><td>Start Date & Time </td><td>' + formatDate(row['startDT']['date']) + ' at ' + formatTime(row['startDT']['time']) + (row['startDT']['UTC'] ? ' (UTC)' : '') + '</td><td></td><td></td></tr>';
      for (var j = 0; j < row['props'].length; j++) {
        var prop = row['props'][j];
        caltable += '<tr style="display: none" class="properties_' + (i+1) + '"><td></td><td>' + prop['propName'] + '</td><td>' + prop['propDescr'] + '</td><td></td><td></td></tr>';
      }
      caltable += '<tr style="display:none" class="grey lighten-1 alarms_' + (i + 1) + '"><th></th><th>Action</th><th>Trigger</th><th>Props</th><th></th><th></tr>';
      for (var j = 0; j < row['alarms'].length; j++) {
        var alarm = row['alarms'][j];
        caltable += '<tr style="display:none" class="alarms_' + (i + 1) + '"><td></td><td>' + alarm['action'] + '</td><td>' + alarm['trigger'] + '</td>' + '<td><a style="cursor: pointer" onclick="$(\'.alarmprops_' + (i+1) + '_' + (j+1) +'\').toggle()">' +  + alarm['numProps'] + '</a></td><td></td><td></td></tr>';
        /* Alarm properties */
        caltable += '<tr style="display:none" class="grey lighten-1 alarmprops_' + (i+1) + '_' + (j+1) + '"><th></th><th></th><th>Prop Name</th><th>Prop Value</th><th></th><th></th></tr>';
        for (var k = 0; k < row['alarms'][j]['props'].length; k++) {
          caltable += '<tr style="display:none" class="grey lighten-3 alarmprops_' + (i+1) + '_' + (j+1) + '"><td></td><td></td><td>' + row['alarms'][j]['props'][k]['propName'] + '</td><td>' + row['alarms'][j]['props'][k]['propDescr'] + '</td><td></td><td></td></tr>';
        }
      }
    }
    caltable += '</tbody></table>';

    let eventForm = '<div class="row"><div class="input-field col s5"><input type="text" ';
    eventForm += 'id="add_event_uid" class="input-field">';
    eventForm += '<label for="add_event_uid">UID</label></div><div class="input-field col s3">';
    eventForm += '<input type="text" id="add_event_dtstart_date" class="datepicker"><label for="add_event_dtstart_date">Start Date</label>';
    eventForm += '</div><div class="input-field col s3"><input type="text" id="add_event_dtstart_time"';
    eventForm += 'class="timepicker"><label for="add_event_dtstart_time">Start Time</label></div><div class="input-field col s1"><button id="createeventbtn" class="btn-floating btn-small grey darken-2" onclick="_createEvent()">+</button></div></div>';
    
    $('#calview').append(caltable + eventForm);
    $('#add_event_dtstart_date').datepicker();
    $('#add_event_dtstart_time').timepicker();

  });
});

function formatDate(datestring) {
  return datestring.substring(0, 4) + '/' + datestring.substring(4, 6) + '/' + datestring.substring(6);
}

function formatTime(timestring) {
  return timestring.substring(0, 2) + ':' + timestring.substring(2, 4) + ':' + timestring.substring(4);
}

function _createEvent() {
  let UID = $('#add_event_uid').val();
  let filename = $('#caldropdown').val().replace('.ics', '');
  
  if (filename.length === 0 || filename === 'Choose a file') {
    pushError('Cannot add event!', 'Bad filename');
    return;
  }

  if (UID.length === 0 || UID.length > 1000 ) {
    pushError('Cannot add event!', 'UID is invalid.');
    return;
  }

  let today = new Date();
  let dtstamp = new String(today.getFullYear()) + new String(today.getMonth() + 1).padStart(2, '0') + new String(today.getDate()).padStart(2, '0') + 'T' + new String(today.getHours()).padStart(2, '0') + new String(today.getMinutes()).padStart(2, '0') + new String(today.getSeconds()).padStart(2, '0');
  let startdate = $('#add_event_dtstart_date').val();
  let starttime = $('#add_event_dtstart_time').val();

  if (startdate.length == 0 || starttime.length == 0) {
    pushError('Cannot add event!', 'Invalid start date/time!');
    return;
  }

  let dtstartraw = new Date(startdate + ' ' + starttime);
  let dtstart = new String(dtstartraw.getFullYear()) + new String(dtstartraw.getMonth() + 1).padStart(2, '0') + new String(dtstartraw.getDate()).padStart(2, '0') + 'T' + new String(dtstartraw.getHours()).padStart(2, '0') + new String(dtstartraw.getMinutes()).padStart(2, '0') + new String(dtstartraw.getSeconds()).padStart(2, '0');

  let json = {
    'UID' : UID,
    'sdate' : dtstart.split('T')[0],
    'stime' : dtstart.split('T')[1],
    'sdate2' : dtstamp.split('T')[0],
    'stime2' : dtstamp.split('T')[1],
    'filename' : filename
  }

  $.post('/addevent', json).done(function(result) {
    fileLog();
    $('#calview').empty();
    alert('Done! Please select ' + filename + '.ics again to see the new event!');
  }).fail(function(err) {
    pushError('Could not upload file', err['responseText']);
  });
}

$('#addevent').click( function() {
  let numEvents = $('#events').children('.event').length;
  let num = numEvents + 1;
  /* Rowdy */
  let eventText = '<div class="event event_' + num + ' grey lighten-5 z-depth-1"><div class="row"><div class="col s12 center-align"><h6>Event ' + num + '</h6></div></div>';
  eventText += '<div class="row"><form class="col s12"><div class="input-field col s8"><textarea placeholder="UID" id="event_' + num + '_uid" type="text" class="materialize-textarea"></textarea>';
  eventText += '<label for="event_' + num + '_uid">UID</label></div>';
  
  eventText += '<div class="input-field col s2"><input type="text" id="event_' + num + '_dtstart_date" class="datepicker">';
  eventText += '<label for="event_' + num + '_dtstart_date">Start Date</label></div>'
  
  eventText += '<div class="input-field col s2"><input type="text" id="event_' + num + '_dtstart_time" class="timepicker">';
  eventText += '<label for="event_' + num + '_dtstart_time">Start Time</label></div>'
  
  /* Deleting event */
  eventText += '</form></div>';
  
  /* Event props */
  eventText += '<div id="event_' + num + '_props"></div><div class="row"><div class="col s10 right-align"><button class="btn-flat" onclick="addprop(\'event_' + num + '_props\', ' + num + ')">ADD PROP</button></div><div class="col s2 right-align"><button class="btn-flat" onclick="$(\'.event_' + num + '\').remove()">Delete Event</button></div></div>';
  eventText += '</div></div>';
  /* Finally..*/
  $('#events').append(eventText);
  /* Update the pickers */
  $('.datepicker').datepicker();
  $('.timepicker').timepicker();

});

function pushError(errorMsg, errorCode) {
  /* A visual to see if there are any erorrs */
  if ( errorCode !== 'OK' && $('#status').hasClass('green') ) {
    $('#status').toggleClass('green red');
  }
  let errorNum = $("#errorList").children().length + 1;
  $('#errorList').append('<li id="error_' + errorNum + '" class="collection-item' + (errorCode === 'OK' ? ' green lighten-3 ' : '') + '">' + errorMsg + ' <b>Code</b>: '  + errorCode + '!</li>');
  $([document.documentElement, document.body]).animate({
    scrollTop: $('#status').offset().top,
  }, 2000, function() {
    $("#errorList").parent().parent().animate({scrollTop: $('ul#errorList li:last').offset().top - 30}, 333, function() {
      for (let i = 0; i < 3; i++) {
        $('#statuspanel').fadeOut(100).fadeIn(100);
      }
    });
 });
}

$('#btnClear').click(function() {
  if ( $('#status').hasClass('red') ) {
    $('#status').toggleClass('green red');
  }
  $('#errorList').empty();
});

function clearCreateCalendar() {
  let numEvents = $('#events').children().length;
  if (numEvents > 1) {
    /* Remove all events */
    for (let i = 2; i <= numEvents; i++) {
      $('.event_' + i).remove();
    }
    /* Clearo out these boys too */
  }
  if ($('#event_1_props').children().length > 0) {
    $('#event_1_props').empty();
  }
  $('#new_name').val("");
  $('#new_prodid').val("");
  $('#event_1_uid').val("");
  $('#event_1_dtstart_date').val('');
  $('#event_1_dtstart_time').val('');
  $('#new_version').val('2');
  $('.datepicker').datepicker();
  $('.timepicker').timepicker();
}

$('#createcalendar').click(function() {
  let name = $('#new_name').val();
  let version = $('#new_version').val();
  let prodId = $('#new_prodid').val();
  let result = {};

  if (name.length === 0 || name.length > 100 || name.includes('.') || name.includes(' ')) {
    pushError('Could not create calendar!', 'Invalid name');
  } else if (version.length === 0 || isNaN(version) ) {
    pushError('Could not create calendar!', 'Invaid version');
  } else if (prodId.length === 0 || prodId.length >  1000) {
    pushError('Could not create calendar!', 'Invalid prodId');
  } else {
    /* So all of these boys are good */
    let result = {
      'name' : name,
      'version' : parseFloat(version),
      'prodId' : prodId,
      'numEvents': 0,
      'events' : []
    };
    /* Now we need grab all the events */
    let numEvents = $('#events').children('.event').length;
    result['numEvents'] = parseInt(numEvents);

    for (let i = 1; i <= numEvents; i++) {
      let uid = $('#event_' + i + '_uid').val();

      if (uid.length === 0 || uid.length > 1000) {
        pushError('Could not create calendar!', 'Invalid UID, Event #' + i);
        return;
      }

      /* Need to check dates */
      let today = new Date();
      let dtstamp = new String(today.getFullYear()) + new String(today.getMonth() + 1).padStart(2, '0') + new String(today.getDate()).padStart(2, '0') + 'T' + new String(today.getHours()).padStart(2, '0') + new String(today.getMinutes()).padStart(2, '0') + new String(today.getSeconds()).padStart(2, '0') + 'Z';
      let startdate = $('#event_' + i + '_dtstart_date').val();
      let starttime = $('#event_' + i + '_dtstart_time').val();
      let dtstartraw = new Date(startdate + ' ' + starttime);
      let dtstart = new String(dtstartraw.getFullYear()) + new String(dtstartraw.getMonth() + 1).padStart(2, '0') + new String(dtstartraw.getDate()).padStart(2, '0') + 'T' + new String(dtstartraw.getHours()).padStart(2, '0') + new String(dtstartraw.getMinutes()).padStart(2, '0') + new String(dtstartraw.getSeconds()).padStart(2, '0') + 'Z';

      let event = {
        'uid' : uid,
        'dtstart' : dtstart,
        'dtstamp' : dtstamp,
        'numProps': 0,
        'props': []
      };

      var numProps = $('#event_' + i + '_props').children().length;
      event['numProps'] = numProps;

      for (let j = 1; j <= numProps; j++) {
        let name = $('#event_' + i + '_' + j + '_propname').val();
        let value = $('#event_' + i + '_' + j + '_propdescr').val();

        if (name.length === 0 || value.length === 0 || name.length > 200) {
          pushError('Could not create calendar!', 'Property ' + j + ' in Event #' + i);
          return;
        }
        event['props'].push({'propName' : name, 'propDescr' : value});
      }

      result['events'].push(event);
    }

    console.log(result);
    $.post('/create', result).done(function(result) {
      fileLog();
      
      $([document.documentElement, document.body]).animate({
        scrollTop: $('#filelogpanel').offset().top,
      }, 2000);

    }).fail(function(err) {
      pushError('Could not upload file', err['responseText']);
    });
  }
});

$('#btnLogin').click(function() {
  $('#login_form').slideToggle();
});

$('#btnConnect').click(function() {
  //gotta double check stuff
  let username = $('#login_username').val();
  let password = $('#login_password').val();
  let dbname = $('#login_dbname').val();
  if (username.length == 0) {
    pushError('Error in connection form!', 'Invalid username length');
    return;
  } else if (password.length == 0) {
    pushError('Error in connection form!', 'Invalid password length');
    return;
  } else if (dbname.length == 0) {
    pushError('Error in connection form!', 'Invalid database name length');
    return;
  }

  let result = {
    'username' : username,
    'password' : password,
    'dbname' : dbname
  }
  $.post('/connect', result).done(function(result) {
    alert('You have connected!');
    $('#dbdropdown').children().remove();
    $('#dbdropdown').append('<li><a id="btnStore" onclick="save_files();">Save Files</a></li><li><a id="btnErase" onclick="delete_files();">Clear</a></li><li><a id="btnStatus" onclick="get_status();">Status</a></li><li><a id="btnQuery" onclick="show_query();">Query</a></li>');
    $.ajax({
      type: "GET",
      url: "/getnumfiles",
      dataType: "JSON",
      success: function(res) {
        //we need to remove these guys if files don't exist on the server
        if (res.numFiles === 0) {
          $('#btnStore').parent().remove();
          $('#btnErase').parent().remove();
        }
      }
    });
    $('#login_form').slideToggle();
    connected = true;
  }).fail(function(err) {
    pushError('Could not connect to ' + dbname + '!', err['responseText']);
  });
});

function show_query() {
  $('#query_form').slideToggle();
}

function save_files() {
  $.ajax({
    type: "GET",
    url: "/saveallfiles",
    success: function(res) {
      pushError('Successfully saved all files to DB!', 'OK');
    },
    error: function(err) {
      pushError('Error!', 'Could not save files to DB');
    }
  });
}

function run_query(query_num) {

  let json = { qnum: query_num };

  let fn = undefined;
  let organizer = undefined;

  if (query_num === 2) {

    fn = $('#query_file').val();

    if (fn.length === 0) {
      pushError('Error in query 2!', 'Invalid file name, make sure it has .ics');
      return;
    }
    
    json['filename'] = fn;
  
  } else if (query_num === 6) {

    organizer = $('#query_organizer').val();

    if (organizer.length === 0) {
      pushError('Error in query 6!', 'Organizer query may not be empty');
      return;
    }

    json['organizer'] = organizer;

  }


  $.post('/query', json).done(function(res) {
    let count = res.length;
    let result = "";

    for (let i = 0; i < count; i++) {

      if (res[i]['isAlarm'] === true) {
        result += 'ALARM #' + res[i].alarm_id + "\n";
        result += '\tParent File: ' + res[i].file_Name + '\n';
      } else {
        result += "EVENT #" + res[i].event_id + "\n";
        result += "\tStart Time: " + res[i].start_time.replace("T", " ").replace("Z", " (UTC)") + "\n";
      }

      if (res[i].action) {
        result += "\tAction: " + res[i].action + "\n";
      }

      if (res[i].trigger) {
        result += "\tTrigger: " + res[i].trigger + "\n";
      }
      
      //does it have a summary?
      if (res[i].summary) {
        result += "\tSummary: " + res[i].summary + "\n";
      }
    
      //default event stuff - at this point I was really tired and the assignment was due very soon :/
      
      if (res[i].location) {
        result += "\tLocation: " + res[i].location + "\n";
      }

      if (res[i].organizer) {
        result += "\tOrganizer: " + res[i].organizer + "\n";
      }
      
    }
    $('#query_textarea').val(result);
    M.textareaAutoResize($('#query_textarea'));

  }).fail(function(err) {
    pushError('Error!', 'There was a problem contacting DB.');
  });
}

function delete_files() {
  $.ajax({
    type: "GET",
    url: "/deleteallfiles",
    success: function(res) {
      pushError('Successfully removed all files from DB!', 'OK');
    },
    error: function(err) {
      pushError('Error!', 'Could not remove files from DB');
    }
  });
}

function get_status() {
  $.ajax({
    type: "GET",
    url: "/dbstatus",
    dataType: "JSON",
    success: function(res) {
      pushError('Database has ' + res['FILE'] + ' files, ' + res['EVENT'] + ' events, and ' + res['ALARM'] + ' alarms.', 'OK');
    },
    error: function(err) {
      pushError(err);
    }
  });
}

$('#btnFile').on('change', function() {
  if ($('#btnFile').val() !== '') {
    var file = new FormData();
    var filename = $('#btnFile').val().replace(/.*[\/\\]/, '');

    file.append("uploadFile", $('#btnFile').prop('files')[0] );
    $.ajax({
      url: '/uploads',
      type: 'POST',
      dataType: 'text',
      data: file,
      cache: false,
      contentType: false,
      processData: false,
      success: function() {
        fileLog();
      },
      error: function(errCode) {
        pushError('Could not upload "' + filename + '"!', errCode['responseText']);
      }
    });
  }
  /* $('#btnFile').val(''); */
});

function addprop ( to, eventnum ) {
  let propNum = $('#' + to).children().length + 1;
  let str = '<div class="row"><div class="col s12"><div class="input-field col s6"><input type="text" id="event_' + eventnum + '_' + propNum + '_propname" class="validate"><label for="event_' + eventnum + '_' + propNum + '_propname">Property Name</label></div>';
  str += '<div class="input-field col s6"><input type="text" id="event_' + eventnum + '_' + propNum + '_propdescr" class="validate"><label for="event_' + eventnum + '_' + propNum + '_propdescr">Property Value</label></div>';
  str += '</div></div>';
  $('#' + to).append(str);
}

/* Updates the file log */
function fileLog(firstTime) {
  /* Clear */
  $('#statusMessage').empty();
  $('#filelogbody').empty();
  $('#caldropdown').empty();
  $('#caldropdown').append('<option value="" disabled selected>Choose a file</option>');
  $.ajax({
    type: "GET",
    url: "/getnumfiles",
    dataType: "JSON",
    success: function(res) {
      //Show how many files there are on the server

      if (res['numFiles'] == 0 ) {
        $('#statusMessage').append('There are <span class="pink-text">NO</span> valid files on the server.');
        $('#btnStore').parent().remove();
        $('#btnErase').parent().remove();
      } else {
        if (connected) {
          $('#statusMessage').append('There are <span class="pink-text">' + res['numFiles'] + '</span> valid files on the server.');
          if ($('#btnStore').length === 0){
            $('#dbdropdown').append('<li><a id="btnStore" onclick="save_files();">Save Files</a></li>');
          }
          if ($('#btnErase').length === 0){
            $('#dbdropdown').append('<li><a id="btnErase" onclick="delete_files();">Clear</a></li>');
          }
        }
      }
      fileCount = res['numFiles'];
      console.log(res);
      //Append them
      res['files'].forEach(obj => {
        /* Show in file log panel only if it's valid */
        calendars[obj['filename']] = obj['eventList'];
        var row = '<tr><td><a href="uploads/' + obj['filename'] +'">' + obj['filename'] + '</a></td><td>' + obj['version'] + '</td><td>' + obj['prodID'] + '</td><td>' + obj['numEvents'] + '</td><td>' + obj['numProps'] + '</td></tr>';
        $('#filelogbody').append(row);
        $('#caldropdown').append('<option value="' + obj['filename'] + '">' + obj['filename'] + '</option>');
      });
      $('select').formSelect();
    },
    complete: function(data) {
      $('#statuspanel').css({
        'height' : $('#fileLog').height() + 'px',
        'overflow' : 'scroll'
      });
    }
  });
}
