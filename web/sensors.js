/**
 * Turns sensor state into what the dashboard shows. Matches SensorDisplay.cpp on the hub.
 *
 * Having a sensor and having a reading are separate: a sensor that hasn't reported yet
 * shows as "--", while a room with no sensor shows nothing.
 */
(function (root) {
    const isNumber = value => typeof value === 'number' && Number.isFinite(value);

    // Sensors report Celsius
    const temperatureF = state => (isNumber(state?.temperature) ? state.temperature * 9 / 5 + 32 : undefined);
    const humidityPercent = state => (isNumber(state?.humidity) ? state.humidity : undefined);

    const temperatureText = (fahrenheit, decimals = 0) =>
        (fahrenheit === undefined ? '--°F' : `${fahrenheit.toFixed(decimals)}°F`);
    const humidityText = percent => (percent === undefined ? '--%' : `${Math.round(percent)}%`);

    const reportsTemperature = type => type === 'temperature_sensor' || type === 'multi_sensor';
    // A plain temperature sensor can still report humidity, for instance over Zigbee
    const reportsHumidity = device =>
        device.type === 'humidity_sensor' || device.type === 'multi_sensor' ||
        (device.type === 'temperature_sensor' && humidityPercent(device.state) !== undefined);

    /** One sensor's current value, as { text, colorClass } */
    function sensorValue(sensor) {
        const state = sensor.state;
        const temperature = temperatureF(state);
        const humidity = humidityPercent(state);

        switch (sensor.type) {
            case 'temperature_sensor':
            case 'multi_sensor':
                if (temperature === undefined && humidity === undefined) return { text: '--', colorClass: '' };
                return {
                    text: humidity === undefined
                        ? temperatureText(temperature, 1)
                        : `${temperatureText(temperature, 1)} / ${humidityText(humidity)}`,
                    colorClass: 'temperature'
                };
            case 'humidity_sensor':
                return humidity === undefined
                    ? { text: '--', colorClass: '' }
                    : { text: humidityText(humidity), colorClass: 'humidity' };
            case 'motion_sensor':
                if (typeof state?.motion !== 'boolean') return { text: '--', colorClass: '' };
                return { text: state.motion ? 'Motion' : 'Clear', colorClass: 'motion' };
            case 'contact_sensor':
                if (typeof state?.contact !== 'boolean') return { text: '--', colorClass: '' };
                return { text: state.contact ? 'Closed' : 'Open', colorClass: '' };
            default:
                return { text: '--', colorClass: '' };
        }
    }

    /** What the sensors among a room's devices report */
    function roomSensors(devices) {
        const room = {
            hasTemperatureSensor: false,
            temperatureF: undefined,
            hasHumiditySensor: false,
            humidity: undefined,
            hasMotionSensor: false,
            motionDetected: false
        };
        devices.forEach(device => {
            if (reportsTemperature(device.type)) {
                room.hasTemperatureSensor = true;
                room.temperatureF ??= temperatureF(device.state);
            }
            if (reportsHumidity(device)) {
                room.hasHumiditySensor = true;
                room.humidity ??= humidityPercent(device.state);
            }
            if (device.type === 'motion_sensor') {
                room.hasMotionSensor = true;
                room.motionDetected = room.motionDetected || device.state?.motion === true;
            }
        });
        return room;
    }

    /** A room's climate for a card, such as "72°F / 45%". Empty if the room has no such sensors. */
    function roomClimateText(room) {
        const parts = [];
        if (room.hasTemperatureSensor) parts.push(temperatureText(room.temperatureF));
        if (room.hasHumiditySensor) parts.push(humidityText(room.humidity));
        return parts.join(' / ');
    }

    const api = { sensorValue, roomSensors, roomClimateText, temperatureText, humidityText };
    if (typeof module !== 'undefined' && module.exports) {
        module.exports = api;
    } else {
        root.SensorDisplay = api;
    }
})(this);
