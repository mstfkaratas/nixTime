function fetchWeather(latitude, longitude) {
  var myAPIKey = "50292d96617dbd9f8554e1916221fa93";
  var req = new XMLHttpRequest();

  var config = window.localStorage.getItem("config");
  var temp_unit = "F";
  if (config !== null) {
      config = JSON.parse(config);
      if (config.CONFIG_TEMP_UNIT_KEY !== undefined) {
          temp_unit = String.fromCharCode(config.CONFIG_TEMP_UNIT_KEY);
      }
  }

  req.open('GET', 'http://api.openweathermap.org/data/2.5/weather?' +
    'lat=' + latitude + '&lon=' + longitude + '&cnt=1&appid=' + myAPIKey, true);
  req.onload = function () {
    if (req.readyState === 4) {
      if (req.status === 200) {
        var response = JSON.parse(req.responseText);
        var temperature = response.main.temp;
        var temp_string = "";
        if (temp_unit == "F") {
            temperature = Math.round((temperature - 273.15)* 1.8 + 32);
            temp_string = temperature + "\xB0F";
        } else if (temp_unit == "C") {
            temperature = Math.round(temperature - 273.15);
            temp_string = temperature + "\xB0C";
        } else {
            temperature = Math.round(temperature);
            temp_string = temperature + "K";
        }
        var weather_id = response.weather[0].id;
        var latlng = (Math.abs(Math.round(latitude * 10)) << 16) | (Math.abs(Math.round(longitude * 10)) & 0xffff);
        if (latitude < 0) {
          latlng = latlng | 0x80000000;
        }
        if (longitude < 0) {
          latlng = latlng | 0x00008000;
        }
        var msg = {
          'WEATHER_ICON_KEY': weather_id,
          'WEATHER_TEMPERATURE_KEY': temp_string,
          'WEATHER_LATLNG_KEY': latlng,
          "CONFIG_TEMP_UNIT_KEY": temp_unit.charCodeAt(0),
        };
        console.log(JSON.stringify(msg));
        Pebble.sendAppMessage(msg);
      } else {
        console.log('Error from OWM');
      }
    }
  };
  req.send(null);
}

function locationSuccess(pos) {
  var coordinates = pos.coords;
  fetchWeather(coordinates.latitude, coordinates.longitude);
}

function locationError(err) {
  console.warn('location error (' + err.code + '): ' + err.message);
  Pebble.sendAppMessage({
    'WEATHER_TEMPERATURE_KEY': '...',
    'WEATHER_ICON_KEY': 42,
    'WEATHER_LATLNG_KEY': 0,
    'CONFIG_TEMP_UNIT_KEY': -1,
  });
}

var locationOptions = {
  'timeout': 15000,
  'maximumAge': 60000
};

Pebble.addEventListener('ready', function (e) {
  window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError,
    locationOptions);
});

Pebble.addEventListener('appmessage', function (e) {
  window.navigator.geolocation.getCurrentPosition(locationSuccess, locationError,
    locationOptions);
});

Pebble.addEventListener('showConfiguration', function() {
  var url = 'https://files.roguelazer.com/nixTime/html/v1.6/config.html';

  Pebble.openURL(url);
});

Pebble.addEventListener('webviewclosed', function (e) {
  var configData = JSON.parse(decodeURIComponent(e.response));
  var props = {
      "CONFIG_COLOR_KEY": configData.color_theme,
      "CONFIG_TEMP_UNIT_KEY": configData.temp_unit.charCodeAt(0),
      "CONFIG_UPDATE_INTERVAL_KEY": configData.update_interval
  };
  console.log(JSON.stringify(props));
  window.localStorage.setItem("config", JSON.stringify(props));
  Pebble.sendAppMessage(props);
});

