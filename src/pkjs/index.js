var xhrRequest = function(url, type, callback) {
    var xhr = new XMLHttpRequest();
    xhr.onload = function() {
        callback(this.responseText);
    };
    xhr.open(type, url);
    xhr.send();
};


function locationSuccess(pos) {
    var url = 'http://api.openweathermap.org/data/2.5/weather?lat=' +
          pos.coords.latitude + '&lon=' + pos.coords.longitude + '&appid=' + 'c88a92d74ad04c28efeb18301300a838';

    /*
    // Send request to OpenWeatherMap
    xhrRequest(url, 'GET',
        function(responseText) {
            // responseText contains a JSON object with weather info
            var json = JSON.parse(responseText);

            // Temp in Kelvin requires adjustment
            var temperature = Math.round(json.main.temp - 273.15);
            console.log('Temperature is ' + temperature);

            // Conditions
            var conditions = json.weather[0].main;
            console.log('Conditions are ' + conditions);

            // Assemble dictionary using our keys
            var dictionary = {
                "TEMPERATURE":temperature,
                "CONDITIONS":conditions
            };

            // Send to Pebble
            Pebble.sendAppMessage(dictionary,
                function(e) {
                    console.log("Weather info sent to Pebble successfully!");
                },
                function(e) {
                    console.log("Error sending weather info to Pebble!");
                }
            );
        }
    );
    */

    //var satsURL = 'http://api.zeitkunst.org/sats/pebble/poem/42.294615,71.302342,185';
    // Give longitude in E longitude, coordinate change happens on the server
    var offsetHours = new Date().getTimezoneOffset();
    var satsURL = 'http://api.zeitkunst.org/sats/pebble/poem/' + pos.coords.latitude + "," + pos.coords.longitude + "/" + offsetHours;

    // Send request to OpenWeatherMap
    xhrRequest(satsURL, 'GET',
        function(responseText) {
            // responseText contains a JSON object with weather info
            var sats = JSON.parse(responseText);

            console.log("Lat, long: " + pos.coords.latitude + ", " + pos.coords.longitude);
            console.log(sats["title"]);
            console.log(sats["poem"]);


            // Assemble dictionary using our keys
            var dictionary = {
                "TITLE":sats["title"],
                "POEM":sats["poem"]
            };

            // Send to Pebble
            Pebble.sendAppMessage(dictionary,
                function(e) {
                    console.log("Poem sent to Pebble successfully!");
                },
                function(e) {
                    console.log("Error sending poem to Pebble: " + JSON.stringify(e));
                }
            );


        }
    );

}

function locationError(err) {
    console.log("Error requesting location!");
}

function getWeather() {
    navigator.geolocation.getCurrentPosition(
        locationSuccess,
        locationError,
        {timeout: 15000, maximumAge: 60000}
    );
}

// Listen for when the watchface is opened
Pebble.addEventListener('ready',
    function(e) {
        console.log('PebbleKit JS ready!');

        // Get the initial weather
        getWeather();
    }
);

// Listen for when an AppMessage is received
Pebble.addEventListener('appmessage',
    function(e) {
        console.log('AppMessage received!');
        getWeather();
    }
);

