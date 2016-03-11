function fetchWeather(latitude, longitude) {
  var myAPIKey = "50292d96617dbd9f8554e1916221fa93";
  var req = new XMLHttpRequest();
  req.open('GET', 'http://api.openweathermap.org/data/2.5/weather?' +
    'lat=' + latitude + '&lon=' + longitude + '&cnt=1&appid=' + myAPIKey, true);
  req.onload = function () {
    if (req.readyState === 4) {
      if (req.status === 200) {
        var response = JSON.parse(req.responseText);
        var temperature = response.main.temp - 273.15;
        temperature = Math.round(temperature * 1.8 + 32);
        var weather_id = response.weather[0].id;
        var msg = {
          'WEATHER_ICON_KEY': weather_id,
          'WEATHER_TEMPERATURE_KEY': temperature + '\xB0F',
        };
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
    'WEATHER_TEMPERATURE_KEY': 'N/A'
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

Pebble.addEventListener('webviewclosed', function (e) {
  console.log('webview closed');
});

