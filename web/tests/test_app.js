/**
 * SmartHub Web Dashboard Tests
 *
 * Run these tests by opening test_runner.html in a browser
 * or using Node.js with jsdom for headless testing.
 */

const SensorDisplayApi = typeof module !== 'undefined' && module.exports
    ? require('../sensors.js')
    : window.SensorDisplay;

// Simple test framework
const TestRunner = {
    tests: [],
    passed: 0,
    failed: 0,

    describe(name, fn) {
        console.group(`📦 ${name}`);
        fn();
        console.groupEnd();
    },

    it(name, fn) {
        this.tests.push({ name, fn });
    },

    async run() {
        console.log('🧪 Running SmartHub Web Dashboard Tests\n');

        for (const test of this.tests) {
            try {
                await test.fn();
                console.log(`  ✅ ${test.name}`);
                this.passed++;
            } catch (error) {
                console.error(`  ❌ ${test.name}`);
                console.error(`     ${error.message}`);
                this.failed++;
            }
        }

        console.log(`\n📊 Results: ${this.passed} passed, ${this.failed} failed`);
        return this.failed === 0;
    },

    assertEqual(actual, expected, message = '') {
        if (actual !== expected) {
            throw new Error(`${message} Expected ${expected}, got ${actual}`);
        }
    },

    assertTrue(value, message = '') {
        if (!value) {
            throw new Error(`${message} Expected true, got ${value}`);
        }
    },

    assertFalse(value, message = '') {
        if (value) {
            throw new Error(`${message} Expected false, got ${value}`);
        }
    },

    assertDefined(value, message = '') {
        if (value === undefined || value === null) {
            throw new Error(`${message} Expected defined value, got ${value}`);
        }
    },

    assertContains(array, item, message = '') {
        if (!array.includes(item)) {
            throw new Error(`${message} Expected array to contain ${item}`);
        }
    }
};

// Mock fetch for testing
const mockDevices = [
    {
        id: 'light-living',
        name: 'Living Room Light',
        type: 'dimmer',
        room: 'Living Room',
        online: true,
        state: { on: true, brightness: 80 }
    },
    {
        id: 'light-bedroom',
        name: 'Bedroom Light',
        type: 'color_light',
        room: 'Bedroom',
        online: true,
        state: { on: false, brightness: 50 }
    },
    {
        id: 'switch-kitchen',
        name: 'Kitchen Switch',
        type: 'switch',
        room: 'Kitchen',
        online: true,
        state: { on: true }
    },
    {
        id: 'temp-living',
        name: 'Living Room Temp',
        type: 'temperature_sensor',
        room: 'Living Room',
        online: true,
        state: { temperature: 22.5 }
    },
    {
        id: 'humidity-bath',
        name: 'Bathroom Humidity',
        type: 'humidity_sensor',
        room: 'Bathroom',
        online: true,
        state: { humidity: 65 }
    }
];

function createMockFetch() {
    return function mockFetch(url, options = {}) {
        return new Promise((resolve) => {
            let response = { ok: true, status: 200 };

            if (url === '/api/devices') {
                response.json = () => Promise.resolve(mockDevices);
            } else if (url.startsWith('/api/devices/')) {
                const id = url.split('/').pop();
                const device = mockDevices.find(d => d.id === id);
                if (device) {
                    response.json = () => Promise.resolve(device);
                } else {
                    response.ok = false;
                    response.status = 404;
                    response.json = () => Promise.resolve({ error: 'Not found' });
                }
            } else if (url === '/api/auth/me') {
                response.ok = false;
                response.status = 401;
                response.json = () => Promise.resolve({ error: 'Unauthorized' });
            }

            resolve(response);
        });
    };
}

// ============================================================================
// Device Classification Tests
// ============================================================================

TestRunner.describe('Device Classification', () => {
    TestRunner.it('should identify sensor devices correctly', () => {
        const sensorTypes = ['temperature_sensor', 'humidity_sensor', 'motion_sensor', 'contact_sensor'];
        const isSensor = (type) => type && type.includes('sensor');

        for (const type of sensorTypes) {
            TestRunner.assertTrue(isSensor(type), `${type} should be a sensor`);
        }
    });

    TestRunner.it('should identify controllable devices correctly', () => {
        const deviceTypes = ['dimmer', 'switch', 'color_light'];
        const isSensor = (type) => type && type.includes('sensor');

        for (const type of deviceTypes) {
            TestRunner.assertFalse(isSensor(type), `${type} should not be a sensor`);
        }
    });

    TestRunner.it('should separate devices and sensors from mock data', () => {
        const devices = mockDevices.filter(d => !d.type.includes('sensor'));
        const sensors = mockDevices.filter(d => d.type.includes('sensor'));

        TestRunner.assertEqual(devices.length, 3, 'Should have 3 devices');
        TestRunner.assertEqual(sensors.length, 2, 'Should have 2 sensors');
    });
});

// ============================================================================
// Room Grouping Tests
// ============================================================================

TestRunner.describe('Room Grouping', () => {
    TestRunner.it('should extract unique rooms from devices', () => {
        const rooms = new Set(mockDevices.map(d => d.room));

        TestRunner.assertEqual(rooms.size, 4, 'Should have 4 unique rooms');
        TestRunner.assertTrue(rooms.has('Living Room'));
        TestRunner.assertTrue(rooms.has('Bedroom'));
        TestRunner.assertTrue(rooms.has('Kitchen'));
        TestRunner.assertTrue(rooms.has('Bathroom'));
    });

    TestRunner.it('should group devices by room', () => {
        const roomData = {};
        mockDevices.forEach(device => {
            const room = device.room || 'Other';
            if (!roomData[room]) roomData[room] = [];
            roomData[room].push(device);
        });

        TestRunner.assertEqual(roomData['Living Room'].length, 2);
        TestRunner.assertEqual(roomData['Bedroom'].length, 1);
        TestRunner.assertEqual(roomData['Kitchen'].length, 1);
        TestRunner.assertEqual(roomData['Bathroom'].length, 1);
    });
});

// ============================================================================
// Device State Tests
// ============================================================================

TestRunner.describe('Device State', () => {
    TestRunner.it('should read on/off state from device', () => {
        const light = mockDevices.find(d => d.id === 'light-living');
        TestRunner.assertTrue(light.state.on, 'Living room light should be on');

        const bedroomLight = mockDevices.find(d => d.id === 'light-bedroom');
        TestRunner.assertFalse(bedroomLight.state.on, 'Bedroom light should be off');
    });

    TestRunner.it('should read brightness from dimmer devices', () => {
        const light = mockDevices.find(d => d.id === 'light-living');
        TestRunner.assertEqual(light.state.brightness, 80);
    });

    TestRunner.it('should read sensor values', () => {
        const temp = mockDevices.find(d => d.id === 'temp-living');
        TestRunner.assertEqual(temp.state.temperature, 22.5);

        const humidity = mockDevices.find(d => d.id === 'humidity-bath');
        TestRunner.assertEqual(humidity.state.humidity, 65);
    });

    TestRunner.it('should count active devices', () => {
        const devices = mockDevices.filter(d => !d.type.includes('sensor'));
        const activeCount = devices.filter(d => d.state?.on).length;

        TestRunner.assertEqual(activeCount, 2, 'Should have 2 active devices');
    });
});

// ============================================================================
// Icon Mapping Tests
// ============================================================================

TestRunner.describe('Icon Mapping', () => {
    const getDeviceIcon = (type) => {
        const icons = {
            'dimmer': '💡',
            'color_light': '🌈',
            'switch': '🔌',
            'light': '💡'
        };
        return icons[type] || '📟';
    };

    const getSensorIcon = (type) => {
        if (type.includes('temperature')) return '🌡️';
        if (type.includes('humidity')) return '💧';
        if (type.includes('motion')) return '👁️';
        if (type.includes('contact')) return '🚪';
        return '📊';
    };

    TestRunner.it('should return correct device icons', () => {
        TestRunner.assertEqual(getDeviceIcon('dimmer'), '💡');
        TestRunner.assertEqual(getDeviceIcon('color_light'), '🌈');
        TestRunner.assertEqual(getDeviceIcon('switch'), '🔌');
        TestRunner.assertEqual(getDeviceIcon('unknown'), '📟');
    });

    TestRunner.it('should return correct sensor icons', () => {
        TestRunner.assertEqual(getSensorIcon('temperature_sensor'), '🌡️');
        TestRunner.assertEqual(getSensorIcon('humidity_sensor'), '💧');
        TestRunner.assertEqual(getSensorIcon('motion_sensor'), '👁️');
        TestRunner.assertEqual(getSensorIcon('contact_sensor'), '🚪');
    });
});

// ============================================================================
// Sensor Value Formatting Tests
// ============================================================================

TestRunner.describe('Sensor Value Formatting', () => {
    const { sensorValue, roomSensors, roomClimateText, roomMotionText } = SensorDisplayApi;

    TestRunner.it('should convert temperature from the Celsius sensors report', () => {
        const result = sensorValue({ type: 'temperature_sensor', state: { temperature: 22.5 } });
        TestRunner.assertEqual(result.text, '72.5°F');
        TestRunner.assertEqual(result.colorClass, 'temperature');
    });

    TestRunner.it('should show a real 0°C rather than treat it as no reading', () => {
        TestRunner.assertEqual(sensorValue({ type: 'temperature_sensor', state: { temperature: 0 } }).text, '32.0°F');
    });

    TestRunner.it('should show no reading before a sensor first reports', () => {
        ['temperature_sensor', 'multi_sensor', 'humidity_sensor', 'motion_sensor', 'contact_sensor'].forEach(type => {
            TestRunner.assertEqual(sensorValue({ type, state: {} }).text, '--');
        });
        TestRunner.assertEqual(sensorValue({ type: 'temperature_sensor' }).text, '--');
    });

    TestRunner.it('should show temperature and humidity from a combined sensor', () => {
        TestRunner.assertEqual(
            sensorValue({ type: 'multi_sensor', state: { temperature: 20, humidity: 45.4 } }).text, '68.0°F / 45%');
        TestRunner.assertEqual(sensorValue({ type: 'multi_sensor', state: { humidity: 45 } }).text, '--°F / 45%');
    });

    TestRunner.it('should format humidity values', () => {
        const result = sensorValue({ type: 'humidity_sensor', state: { humidity: 65.7 } });
        TestRunner.assertEqual(result.text, '66%');
        TestRunner.assertEqual(result.colorClass, 'humidity');
    });

    TestRunner.it('should read motion from the motion property', () => {
        TestRunner.assertEqual(sensorValue({ type: 'motion_sensor', state: { motion: true } }).text, 'Motion');
        TestRunner.assertEqual(sensorValue({ type: 'motion_sensor', state: { motion: false } }).text, 'Clear');
    });

    TestRunner.it('should read contact from the contact property', () => {
        TestRunner.assertEqual(sensorValue({ type: 'contact_sensor', state: { contact: true } }).text, 'Closed');
        TestRunner.assertEqual(sensorValue({ type: 'contact_sensor', state: { contact: false } }).text, 'Open');
    });

    TestRunner.it('should show nothing on a room card without sensors', () => {
        const room = roomSensors([{ type: 'switch', state: { on: true } }]);
        TestRunner.assertEqual(roomClimateText(room), '');
    });

    TestRunner.it('should show a sensor that has not reported as no reading on a room card', () => {
        TestRunner.assertEqual(roomClimateText(roomSensors([{ type: 'temperature_sensor', state: {} }])), '--°F');
    });

    TestRunner.it('should show a humidity-only room without a temperature', () => {
        const room = roomSensors([{ type: 'humidity_sensor', state: { humidity: 50 } }]);
        TestRunner.assertFalse(room.hasTemperatureSensor);
        TestRunner.assertEqual(roomClimateText(room), '50%');
    });

    TestRunner.it('should use the sensor with a reading when a room has several', () => {
        const room = roomSensors([
            { type: 'temperature_sensor', state: {} },
            { type: 'temperature_sensor', state: { temperature: 20 } }
        ]);
        TestRunner.assertEqual(roomClimateText(room), '68°F');
    });

    TestRunner.it('should not call a room clear before its motion sensor reports', () => {
        const room = roomSensors([{ type: 'motion_sensor', state: {} }]);
        TestRunner.assertTrue(room.hasMotionSensor);
        TestRunner.assertEqual(roomMotionText(room), '--');
        TestRunner.assertEqual(roomMotionText(roomSensors([{ type: 'motion_sensor', state: { motion: false } }])), 'Clear');
    });

    TestRunner.it('should round the same way as the LCD', () => {
        // Compare with SensorDisplay.RoundsTheSameWayAsTheWebDashboard on the hub
        TestRunner.assertEqual(roomClimateText(roomSensors([{ type: 'temperature_sensor', state: { temperature: 22.5 } }])), '73°F');
        TestRunner.assertEqual(sensorValue({ type: 'humidity_sensor', state: { humidity: 44.5 } }).text, '45%');
        TestRunner.assertEqual(roomClimateText(roomSensors([{ type: 'temperature_sensor', state: { temperature: -17.8 } }])), '0°F');
    });

    TestRunner.it('should detect motion from any sensor in a room', () => {
        const room = roomSensors([
            { type: 'motion_sensor', state: { motion: false } },
            { type: 'motion_sensor', state: { motion: true } }
        ]);
        TestRunner.assertTrue(room.hasMotionSensor);
        TestRunner.assertTrue(room.motionDetected);
    });
});

// ============================================================================
// Statistics Calculation Tests
// ============================================================================

TestRunner.describe('Statistics Calculation', () => {
    TestRunner.it('should calculate total device count', () => {
        const devices = mockDevices.filter(d => !d.type.includes('sensor'));
        TestRunner.assertEqual(devices.length, 3);
    });

    TestRunner.it('should calculate total sensor count', () => {
        const sensors = mockDevices.filter(d => d.type.includes('sensor'));
        TestRunner.assertEqual(sensors.length, 2);
    });

    TestRunner.it('should calculate active device count', () => {
        const devices = mockDevices.filter(d => !d.type.includes('sensor'));
        const active = devices.filter(d => d.state?.on).length;
        TestRunner.assertEqual(active, 2);
    });

    TestRunner.it('should calculate room count', () => {
        const rooms = new Set(mockDevices.map(d => d.room));
        TestRunner.assertEqual(rooms.size, 4);
    });
});

// ============================================================================
// Device Filtering Tests
// ============================================================================

TestRunner.describe('Device Filtering', () => {
    TestRunner.it('should filter devices by room', () => {
        const filterByRoom = (devices, room) => {
            if (!room) return devices;
            return devices.filter(d => d.room === room);
        };

        const livingRoom = filterByRoom(mockDevices, 'Living Room');
        TestRunner.assertEqual(livingRoom.length, 2);

        const kitchen = filterByRoom(mockDevices, 'Kitchen');
        TestRunner.assertEqual(kitchen.length, 1);

        const all = filterByRoom(mockDevices, '');
        TestRunner.assertEqual(all.length, 5);
    });

    TestRunner.it('should return empty for nonexistent room', () => {
        const filtered = mockDevices.filter(d => d.room === 'Garage');
        TestRunner.assertEqual(filtered.length, 0);
    });
});

// ============================================================================
// API Mock Tests
// ============================================================================

TestRunner.describe('API Integration', () => {
    TestRunner.it('should fetch devices list', async () => {
        const fetch = createMockFetch();
        const response = await fetch('/api/devices');

        TestRunner.assertTrue(response.ok);
        const data = await response.json();
        TestRunner.assertEqual(data.length, 5);
    });

    TestRunner.it('should fetch single device', async () => {
        const fetch = createMockFetch();
        const response = await fetch('/api/devices/light-living');

        TestRunner.assertTrue(response.ok);
        const data = await response.json();
        TestRunner.assertEqual(data.id, 'light-living');
        TestRunner.assertEqual(data.name, 'Living Room Light');
    });

    TestRunner.it('should return 404 for unknown device', async () => {
        const fetch = createMockFetch();
        const response = await fetch('/api/devices/nonexistent');

        TestRunner.assertFalse(response.ok);
        TestRunner.assertEqual(response.status, 404);
    });

    TestRunner.it('should return 401 for auth endpoint without login', async () => {
        const fetch = createMockFetch();
        const response = await fetch('/api/auth/me');

        TestRunner.assertFalse(response.ok);
        TestRunner.assertEqual(response.status, 401);
    });
});

// ============================================================================
// Tab Navigation Tests
// ============================================================================

TestRunner.describe('Tab Navigation', () => {
    TestRunner.it('should identify valid tab names', () => {
        const validTabs = ['dashboard', 'devices', 'sensors'];

        validTabs.forEach(tab => {
            TestRunner.assertTrue(
                ['dashboard', 'devices', 'sensors'].includes(tab),
                `${tab} should be a valid tab`
            );
        });
    });

    TestRunner.it('should map tab to content id', () => {
        const getContentId = (tab) => `tab-${tab}`;

        TestRunner.assertEqual(getContentId('dashboard'), 'tab-dashboard');
        TestRunner.assertEqual(getContentId('devices'), 'tab-devices');
        TestRunner.assertEqual(getContentId('sensors'), 'tab-sensors');
    });
});

// ============================================================================
// Brightness Control Tests
// ============================================================================

TestRunner.describe('Brightness Control', () => {
    TestRunner.it('should identify devices with brightness control', () => {
        const hasBrightness = (type) => type === 'dimmer' || type === 'color_light';

        TestRunner.assertTrue(hasBrightness('dimmer'));
        TestRunner.assertTrue(hasBrightness('color_light'));
        TestRunner.assertFalse(hasBrightness('switch'));
    });

    TestRunner.it('should validate brightness range', () => {
        const clampBrightness = (value) => Math.max(0, Math.min(100, value));

        TestRunner.assertEqual(clampBrightness(50), 50);
        TestRunner.assertEqual(clampBrightness(150), 100);
        TestRunner.assertEqual(clampBrightness(-10), 0);
    });

    TestRunner.it('should format brightness display', () => {
        const formatBrightness = (value) => `${value}%`;

        TestRunner.assertEqual(formatBrightness(80), '80%');
        TestRunner.assertEqual(formatBrightness(0), '0%');
        TestRunner.assertEqual(formatBrightness(100), '100%');
    });
});

// Run tests if in browser or Node.js
if (typeof window !== 'undefined') {
    window.addEventListener('DOMContentLoaded', () => {
        TestRunner.run().then(success => {
            document.body.innerHTML = `
                <h1>SmartHub Web Dashboard Tests</h1>
                <p>Check the browser console for detailed results.</p>
                <p style="color: ${success ? 'green' : 'red'}">
                    ${success ? '✅ All tests passed!' : '❌ Some tests failed'}
                </p>
            `;
        });
    });
} else if (typeof module !== 'undefined') {
    module.exports = { TestRunner, mockDevices };
    TestRunner.run().then(success => {
        process.exit(success ? 0 : 1);
    });
}
