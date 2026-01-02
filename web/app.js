// SmartHub Dashboard JavaScript

class SmartHubApp {
    constructor() {
        this.devices = [];
        this.sensors = [];
        this.rooms = new Set();
        this.currentUser = null;
        this.refreshInterval = null;

        this.init();
    }

    init() {
        // Check if already logged in
        this.checkAuth();

        // Setup event listeners
        this.setupEventListeners();
    }

    setupEventListeners() {
        // Login form
        document.getElementById('login-form').addEventListener('submit', (e) => {
            e.preventDefault();
            this.login();
        });

        // Logout button
        document.getElementById('logout-btn').addEventListener('click', () => {
            this.logout();
        });

        // Tab navigation
        document.querySelectorAll('.tab').forEach(tab => {
            tab.addEventListener('click', (e) => {
                this.switchTab(e.target.dataset.tab);
            });
        });

        // Room filter
        document.getElementById('room-filter').addEventListener('change', (e) => {
            this.filterDevices(e.target.value);
        });
    }

    async checkAuth() {
        try {
            const response = await fetch('/api/auth/me', {
                credentials: 'include'
            });

            if (response.ok) {
                this.currentUser = await response.json();
                this.showDashboard();
            } else {
                // For demo: skip login and go directly to dashboard
                this.currentUser = { username: 'Demo User' };
                this.showDashboard();
            }
        } catch (error) {
            // For demo: skip login and go directly to dashboard
            this.currentUser = { username: 'Demo User' };
            this.showDashboard();
        }
    }

    async login() {
        const username = document.getElementById('username').value;
        const password = document.getElementById('password').value;
        const errorEl = document.getElementById('login-error');

        try {
            const response = await fetch('/api/auth/login', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ username, password }),
                credentials: 'include'
            });

            if (response.ok) {
                this.currentUser = { username };
                errorEl.textContent = '';
                this.showDashboard();
            } else {
                const data = await response.json();
                errorEl.textContent = data.error || 'Login failed';
            }
        } catch (error) {
            errorEl.textContent = 'Connection error';
        }
    }

    async logout() {
        try {
            await fetch('/api/auth/logout', {
                method: 'POST',
                credentials: 'include'
            });
        } catch (error) {
            console.error('Logout error:', error);
        }

        this.currentUser = null;
        this.stopRefresh();
        this.showLogin();
    }

    showLogin() {
        document.getElementById('login-screen').classList.remove('hidden');
        document.getElementById('dashboard-screen').classList.add('hidden');
    }

    showDashboard() {
        document.getElementById('login-screen').classList.add('hidden');
        document.getElementById('dashboard-screen').classList.remove('hidden');

        if (this.currentUser) {
            document.getElementById('user-name').textContent = this.currentUser.username || 'User';
        }

        this.loadData();
        this.startRefresh();
    }

    startRefresh() {
        this.refreshInterval = setInterval(() => {
            this.loadData();
        }, 5000);
    }

    stopRefresh() {
        if (this.refreshInterval) {
            clearInterval(this.refreshInterval);
            this.refreshInterval = null;
        }
    }

    async loadData() {
        try {
            const response = await fetch('/api/devices', {
                credentials: 'include'
            });

            if (response.ok) {
                const devices = await response.json();
                await this.processDevices(devices);
                this.render();
            } else if (response.status === 401) {
                this.logout();
            }
        } catch (error) {
            console.error('Failed to load devices:', error);
        }
    }

    async processDevices(devices) {
        this.devices = [];
        this.sensors = [];
        this.rooms = new Set();

        for (const device of devices) {
            // Fetch full device details
            try {
                const response = await fetch(`/api/devices/${device.id}`, {
                    credentials: 'include'
                });

                if (response.ok) {
                    const fullDevice = await response.json();

                    // Categorize device
                    if (this.isSensor(fullDevice.type)) {
                        this.sensors.push(fullDevice);
                    } else {
                        this.devices.push(fullDevice);
                    }

                    if (fullDevice.room) {
                        this.rooms.add(fullDevice.room);
                    }
                }
            } catch (error) {
                console.error(`Failed to fetch device ${device.id}:`, error);
            }
        }
    }

    isSensor(type) {
        return type && (
            type.includes('sensor') ||
            type.includes('temperature') ||
            type.includes('humidity') ||
            type.includes('motion') ||
            type.includes('contact')
        );
    }

    render() {
        this.renderStats();
        this.renderRooms();
        this.renderDevices();
        this.renderSensors();
        this.renderRoomFilter();
    }

    renderStats() {
        const totalDevices = this.devices.length + this.sensors.length;
        const activeDevices = this.devices.filter(d => d.state?.on).length;

        document.getElementById('total-devices').textContent = this.devices.length;
        document.getElementById('active-devices').textContent = activeDevices;
        document.getElementById('total-sensors').textContent = this.sensors.length;
        document.getElementById('total-rooms').textContent = this.rooms.size;
    }

    renderRooms() {
        const container = document.getElementById('rooms-grid');
        container.innerHTML = '';

        const roomData = {};

        // Group devices by room
        [...this.devices, ...this.sensors].forEach(device => {
            const room = device.room || 'Other';
            if (!roomData[room]) {
                roomData[room] = { devices: [], sensors: [], temp: null };
            }

            if (this.isSensor(device.type)) {
                roomData[room].sensors.push(device);
                if (device.type.includes('temperature') && device.state?.value) {
                    roomData[room].temp = device.state.value;
                }
            } else {
                roomData[room].devices.push(device);
            }
        });

        // Create room cards
        Object.entries(roomData).forEach(([roomName, data]) => {
            const card = document.createElement('div');
            card.className = 'room-card';

            const activeCount = data.devices.filter(d => d.state?.on).length;
            const tempDisplay = data.temp ? `${data.temp}°` : '';

            card.innerHTML = `
                <div class="room-header">
                    <div class="room-name">${roomName}</div>
                    <div class="room-temp">${tempDisplay}</div>
                </div>
                <div class="room-devices">
                    ${data.devices.map(d => `
                        <div class="room-device ${d.state?.on ? 'on' : ''}">
                            ${this.getDeviceIcon(d.type)}
                            <span>${d.name.split(' ').slice(-1)[0]}</span>
                        </div>
                    `).join('')}
                </div>
            `;

            container.appendChild(card);
        });
    }

    renderDevices() {
        const container = document.getElementById('devices-list');
        container.innerHTML = '';

        this.devices.forEach(device => {
            const card = document.createElement('div');
            card.className = 'device-card';
            card.dataset.room = device.room || '';

            const isOn = device.state?.on || false;
            const brightness = device.state?.brightness;
            const hasBrightness = device.type === 'dimmer' || device.type === 'color_light';

            card.innerHTML = `
                <div class="device-info">
                    <div class="device-icon ${isOn ? 'on' : ''}">
                        ${this.getDeviceIcon(device.type)}
                    </div>
                    <div class="device-details">
                        <h3>${device.name}</h3>
                        <div class="device-meta">${device.room || 'No room'} • ${device.type}</div>
                    </div>
                </div>
                <div class="device-controls">
                    ${hasBrightness ? `
                        <div class="brightness-control">
                            <input type="range" class="brightness-slider"
                                min="0" max="100" value="${brightness || 100}"
                                data-device-id="${device.id}">
                            <span class="brightness-value">${brightness || 100}%</span>
                        </div>
                    ` : ''}
                    <label class="toggle">
                        <input type="checkbox" ${isOn ? 'checked' : ''}
                            data-device-id="${device.id}">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
            `;

            // Add event listeners
            const toggle = card.querySelector('.toggle input');
            toggle.addEventListener('change', (e) => {
                this.toggleDevice(device.id, e.target.checked);
            });

            const slider = card.querySelector('.brightness-slider');
            if (slider) {
                slider.addEventListener('input', (e) => {
                    const value = e.target.value;
                    card.querySelector('.brightness-value').textContent = `${value}%`;
                });
                slider.addEventListener('change', (e) => {
                    this.setBrightness(device.id, parseInt(e.target.value));
                });
            }

            container.appendChild(card);
        });
    }

    renderSensors() {
        const container = document.getElementById('sensors-grid');
        container.innerHTML = '';

        this.sensors.forEach(sensor => {
            const card = document.createElement('div');
            card.className = 'sensor-card';

            const value = sensor.state?.value;
            const { displayValue, unit, colorClass } = this.formatSensorValue(sensor.type, value);

            card.innerHTML = `
                <div class="sensor-header">
                    <div>
                        <div class="sensor-name">${sensor.name}</div>
                        <div class="sensor-room">${sensor.room || 'No room'}</div>
                    </div>
                    <div>${this.getSensorIcon(sensor.type)}</div>
                </div>
                <div class="sensor-value ${colorClass}">${displayValue}${unit}</div>
                <div class="sensor-updated">Updated just now</div>
            `;

            container.appendChild(card);
        });
    }

    renderRoomFilter() {
        const select = document.getElementById('room-filter');
        const currentValue = select.value;

        select.innerHTML = '<option value="">All Rooms</option>';

        [...this.rooms].sort().forEach(room => {
            const option = document.createElement('option');
            option.value = room;
            option.textContent = room;
            select.appendChild(option);
        });

        select.value = currentValue;
    }

    filterDevices(room) {
        const cards = document.querySelectorAll('.device-card');
        cards.forEach(card => {
            if (!room || card.dataset.room === room) {
                card.style.display = '';
            } else {
                card.style.display = 'none';
            }
        });
    }

    async toggleDevice(deviceId, on) {
        try {
            const response = await fetch(`/api/devices/${deviceId}`, {
                method: 'PUT',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ on }),
                credentials: 'include'
            });

            if (response.ok) {
                // Update local state
                const device = this.devices.find(d => d.id === deviceId);
                if (device) {
                    device.state = device.state || {};
                    device.state.on = on;
                }
                this.showToast(`${on ? 'Turned on' : 'Turned off'}`, 'success');
            } else {
                this.showToast('Failed to update device', 'error');
            }
        } catch (error) {
            console.error('Toggle error:', error);
            this.showToast('Connection error', 'error');
        }
    }

    async setBrightness(deviceId, brightness) {
        try {
            const response = await fetch(`/api/devices/${deviceId}`, {
                method: 'PUT',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ brightness }),
                credentials: 'include'
            });

            if (response.ok) {
                const device = this.devices.find(d => d.id === deviceId);
                if (device) {
                    device.state = device.state || {};
                    device.state.brightness = brightness;
                }
            }
        } catch (error) {
            console.error('Brightness error:', error);
        }
    }

    getDeviceIcon(type) {
        const icons = {
            'dimmer': '💡',
            'color_light': '🌈',
            'switch': '🔌',
            'light': '💡',
            'outlet': '🔌',
            'thermostat': '🌡️',
            'fan': '🌀',
            'lock': '🔒',
            'camera': '📷'
        };
        return icons[type] || '📟';
    }

    getSensorIcon(type) {
        if (type.includes('temperature')) return '🌡️';
        if (type.includes('humidity')) return '💧';
        if (type.includes('motion')) return '👁️';
        if (type.includes('contact')) return '🚪';
        if (type.includes('light')) return '☀️';
        return '📊';
    }

    formatSensorValue(type, value) {
        if (value === undefined || value === null) {
            return { displayValue: '--', unit: '', colorClass: '' };
        }

        if (type.includes('temperature')) {
            return { displayValue: value.toFixed(1), unit: '°F', colorClass: 'temperature' };
        }
        if (type.includes('humidity')) {
            return { displayValue: Math.round(value), unit: '%', colorClass: 'humidity' };
        }
        if (type.includes('motion')) {
            return { displayValue: value ? 'Motion' : 'Clear', unit: '', colorClass: 'motion' };
        }
        if (type.includes('contact')) {
            return { displayValue: value ? 'Closed' : 'Open', unit: '', colorClass: '' };
        }

        return { displayValue: value, unit: '', colorClass: '' };
    }

    switchTab(tabName) {
        // Update tab buttons
        document.querySelectorAll('.tab').forEach(tab => {
            tab.classList.toggle('active', tab.dataset.tab === tabName);
        });

        // Update tab content
        document.querySelectorAll('.tab-content').forEach(content => {
            content.classList.toggle('active', content.id === `tab-${tabName}`);
        });
    }

    showToast(message, type = 'info') {
        const toast = document.getElementById('toast');
        toast.textContent = message;
        toast.className = `toast show ${type}`;

        setTimeout(() => {
            toast.classList.remove('show');
        }, 3000);
    }
}

// Initialize app when DOM is ready
document.addEventListener('DOMContentLoaded', () => {
    window.app = new SmartHubApp();
});
