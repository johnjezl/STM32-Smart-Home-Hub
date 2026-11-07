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

    // No "-0°F"
    const fixed = (value, decimals) => {
        const rounded = Number(value.toFixed(decimals));
        return (rounded === 0 ? 0 : rounded).toFixed(decimals);
    };
    const temperatureText = (fahrenheit, decimals = 0) =>
        (fahrenheit === undefined ? '--°F' : `${fixed(fahrenheit, decimals)}°F`);
    const humidityText = percent => (percent === undefined ? '--%' : `${Math.round(percent)}%`);

    const ALARM_WORDS = { smoke_sensor: 'Smoke', water_sensor: 'Leak', co_sensor: 'CO', alarm_sensor: 'Alarm' };
    const isAlarmSensor = type => Object.hasOwn(ALARM_WORDS, type);
    // Sensors for something else, which may also measure the room
    const mayAlsoMeasure = type => type === 'motion_sensor' || type === 'contact_sensor' || isAlarmSensor(type);

    const reportsTemperature = device =>
        device.type === 'temperature_sensor' || device.type === 'multi_sensor' ||
        (mayAlsoMeasure(device.type) && temperatureF(device.state) !== undefined);
    // A plain temperature sensor can still report humidity, for instance over Zigbee
    const reportsHumidity = device =>
        device.type === 'humidity_sensor' || device.type === 'multi_sensor' ||
        ((device.type === 'temperature_sensor' || mayAlsoMeasure(device.type)) &&
            humidityPercent(device.state) !== undefined);

    /** What's going off, such as "Leak", or empty if this sensor's alarm isn't active */
    const activeAlarmText = sensor =>
        (isAlarmSensor(sensor.type) && sensor.state?.alarm === true ? ALARM_WORDS[sensor.type] : '');

    /** Every active alarm, such as "Leak: Basement, Smoke: Kitchen". Empty if there are none. */
    const activeAlarmsSummary = devices => devices
        .filter(activeAlarmText)
        .map(device => `${activeAlarmText(device)}: ${device.name}`)
        .join(', ');

    function withLight(parts, state) {
        if (isNumber(state?.illuminance)) parts.push(`${fixed(state.illuminance, 0)} lux`);
        return parts.length > 0 ? parts.join(' / ') : '--';
    }

    // What it's for, with any other readings after it
    function withOtherReadings(main, state) {
        const parts = main ? [main] : [];
        const temperature = temperatureF(state);
        const humidity = humidityPercent(state);
        if (temperature !== undefined) parts.push(temperatureText(temperature, 1));
        if (humidity !== undefined) parts.push(humidityText(humidity));
        return withLight(parts, state);
    }

    /** One sensor's current value, as { text, colorClass } */
    function sensorValue(sensor) {
        const state = sensor.state;
        const temperature = temperatureF(state);
        const humidity = humidityPercent(state);

        switch (sensor.type) {
            case 'temperature_sensor':
            case 'multi_sensor':
                if (temperature === undefined && humidity === undefined) return { text: withLight([], state), colorClass: '' };
                return {
                    text: withLight(humidity === undefined
                        ? [temperatureText(temperature, 1)]
                        : [temperatureText(temperature, 1), humidityText(humidity)], state),
                    colorClass: 'temperature'
                };
            case 'humidity_sensor':
                return humidity === undefined
                    ? { text: withLight([], state), colorClass: '' }
                    : { text: withLight([humidityText(humidity)], state), colorClass: 'humidity' };
            case 'motion_sensor':
                if (typeof state?.motion !== 'boolean') return { text: withOtherReadings('', state), colorClass: '' };
                return { text: withOtherReadings(state.motion ? 'Motion' : 'Clear', state), colorClass: 'motion' };
            case 'contact_sensor':
                if (typeof state?.contact !== 'boolean') return { text: withOtherReadings('', state), colorClass: '' };
                return { text: withOtherReadings(state.contact ? 'Closed' : 'Open', state), colorClass: '' };
            case 'smoke_sensor':
            case 'water_sensor':
            case 'co_sensor':
            case 'alarm_sensor':
                if (typeof state?.alarm !== 'boolean') return { text: withOtherReadings('', state), colorClass: '' };
                return {
                    text: withOtherReadings(state.alarm ? ALARM_WORDS[sensor.type] : 'Clear', state),
                    colorClass: state.alarm ? 'alarm' : ''
                };
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
            motionReported: false,
            motionDetected: false,
            hasAlarmSensor: false,
            alarmReported: false,
            activeAlarms: []
        };
        devices.forEach(device => {
            if (reportsTemperature(device)) {
                room.hasTemperatureSensor = true;
                room.temperatureF ??= temperatureF(device.state);
            }
            if (reportsHumidity(device)) {
                room.hasHumiditySensor = true;
                room.humidity ??= humidityPercent(device.state);
            }
            if (device.type === 'motion_sensor') {
                room.hasMotionSensor = true;
                if (typeof device.state?.motion === 'boolean') {
                    room.motionReported = true;
                    room.motionDetected = room.motionDetected || device.state.motion;
                }
            }
            if (isAlarmSensor(device.type)) {
                room.hasAlarmSensor = true;
                room.alarmReported ||= typeof device.state?.alarm === 'boolean';
                const alarm = activeAlarmText(device);
                if (alarm && !room.activeAlarms.includes(alarm)) room.activeAlarms.push(alarm);
            }
        });
        return room;
    }

    /** "Motion" or "Clear", or "--" before any motion sensor in the room has reported */
    function roomMotionText(room) {
        if (!room.motionReported) return '--';
        return room.motionDetected ? 'Motion' : 'Clear';
    }

    /** "Clear" or what's going off, such as "Leak, Smoke". "--" before any of the room's alarm sensors has reported. */
    function roomAlarmText(room) {
        if (room.activeAlarms.length > 0) return room.activeAlarms.join(', ');
        return room.alarmReported ? 'Clear' : '--';
    }

    /** A room's climate for a card, such as "72°F / 45%". Empty if the room has no such sensors. */
    function roomClimateText(room) {
        const parts = [];
        if (room.hasTemperatureSensor) parts.push(temperatureText(room.temperatureF));
        if (room.hasHumiditySensor) parts.push(humidityText(room.humidity));
        return parts.join(' / ');
    }

    const api = {
        sensorValue, roomSensors, roomClimateText, roomMotionText, roomAlarmText, activeAlarmsSummary,
        temperatureText, humidityText
    };
    if (typeof module !== 'undefined' && module.exports) {
        module.exports = api;
    } else {
        root.SensorDisplay = api;
    }
})(this);
