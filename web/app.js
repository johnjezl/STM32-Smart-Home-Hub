// SmartHub Dashboard JavaScript

class SmartHubApp {
    constructor() {
        this.devices = [];
        this.sensors = [];
        this.rooms = [];
        this.automations = [];
        this.currentUser = null;
        this.refreshInterval = null;

        // Device wizard state
        this.deviceWizard = {
            step: 1,
            type: null,
            name: '',
            protocol: 'local',
            room: '',
            config: {}
        };

        // Automation wizard state
        this.automationWizard = {
            step: 1,
            name: '',
            description: '',
            trigger: null,
            actions: []
        };

        // Zigbee pairing state
        this.pairingActive = false;
        this.pairingPollInterval = null;
        this.discoveredDevice = null;

        // Delete confirmation state
        this.deleteCallback = null;

        this.init();
    }

    init() {
        this.checkAuth();
        this.setupEventListeners();
    }

    setupEventListeners() {
        // Login form
        document.getElementById('login-form').addEventListener('submit', (e) => {
            e.preventDefault();
            this.login();
        });

        // First-run setup form
        document.getElementById('setup-form').addEventListener('submit', (e) => {
            e.preventDefault();
            this.createAdmin();
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

        // Modal overlay clicks
        document.getElementById('modal-overlay').addEventListener('click', (e) => {
            if (e.target.id === 'modal-overlay') {
                this.closeAllModals();
            }
        });

        // Modal close buttons
        document.querySelectorAll('.modal-close, .modal-cancel').forEach(btn => {
            btn.addEventListener('click', () => this.closeAllModals());
        });

        // Room management
        document.getElementById('add-room-btn').addEventListener('click', () => this.openAddRoomModal());
        document.getElementById('save-room-btn').addEventListener('click', () => this.saveRoom());
        document.getElementById('update-room-btn').addEventListener('click', () => this.updateRoom());
        document.getElementById('delete-room-btn').addEventListener('click', () => this.confirmDeleteRoom());

        // Device management
        document.getElementById('add-device-btn').addEventListener('click', () => this.openAddDeviceModal());
        document.getElementById('device-prev-btn').addEventListener('click', () => this.deviceWizardPrev());
        document.getElementById('device-next-btn').addEventListener('click', () => this.deviceWizardNext());
        document.getElementById('update-device-btn').addEventListener('click', () => this.updateDevice());
        document.getElementById('delete-device-btn').addEventListener('click', () => this.confirmDeleteDevice());

        // Device type selection
        document.querySelectorAll('.device-type-card').forEach(card => {
            card.addEventListener('click', (e) => {
                document.querySelectorAll('.device-type-card').forEach(c => c.classList.remove('selected'));
                card.classList.add('selected');
                this.deviceWizard.type = card.dataset.type;
            });
        });

        // Protocol selection
        document.getElementById('device-protocol').addEventListener('change', (e) => {
            this.deviceWizard.protocol = e.target.value;
        });

        // Zigbee pairing
        document.getElementById('start-pairing-btn').addEventListener('click', () => this.startZigbeePairing());

        // Automation management
        document.getElementById('add-automation-btn').addEventListener('click', () => this.openAddAutomationModal());
        document.getElementById('automation-prev-btn').addEventListener('click', () => this.automationWizardPrev());
        document.getElementById('automation-next-btn').addEventListener('click', () => this.automationWizardNext());

        // Trigger type selection
        document.getElementById('trigger-type').addEventListener('change', (e) => {
            this.showTriggerConfig(e.target.value);
        });

        // Action management
        document.getElementById('add-action-btn').addEventListener('click', () => {
            document.getElementById('action-form').classList.remove('hidden');
        });
        document.getElementById('confirm-action-btn').addEventListener('click', () => this.addAction());

        // Delete confirmation
        document.getElementById('confirm-delete-btn').addEventListener('click', () => {
            if (this.deleteCallback) {
                this.deleteCallback();
                this.deleteCallback = null;
            }
            this.closeAllModals();
        });

        // Keyboard handling
        document.addEventListener('keydown', (e) => {
            if (e.key === 'Escape') {
                this.closeAllModals();
            }
        });
    }

    // ==================== Authentication ====================

    async checkAuth() {
        try {
            const setup = await fetch('/api/setup/status', { credentials: 'include' });
            if (setup.ok && (await setup.json()).setupRequired) {
                this.showSetup();
                return;
            }

            const response = await fetch('/api/auth/me', { credentials: 'include' });
            if (response.ok) {
                this.currentUser = await response.json();
                this.showDashboard();
            } else if (response.status === 401) {
                this.showLogin();
            } else {
                // The session may be fine, so don't invite another login
                this.showLogin('The hub could not check your login. Reload the page to try again.');
            }
        } catch (error) {
            this.showLogin('Connection error');
        }
    }

    // All authenticated requests go through here, so that a session which has
    // ended takes the user back to the login screen wherever it was noticed.
    async api(url, options = {}) {
        const response = await fetch(url, { credentials: 'include', ...options });
        if (response.status === 401) {
            this.handleSessionEnded();
        }
        return response;
    }

    isAdmin() {
        return this.currentUser?.role === 'admin';
    }

    handleSessionEnded() {
        if (!this.currentUser) {
            return;
        }
        this.endSession();
        this.showLogin('Your session has ended. Please log in again.');
    }

    endSession() {
        this.currentUser = null;
        this.stopRefresh();
        this.closeAllModals();
        this.devices = [];
        this.sensors = [];
        this.rooms = [];
        this.automations = [];
        this.render();  // The next user on this browser shouldn't see any of it
        document.body.classList.remove('is-admin');
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

            const data = await response.json();
            if (response.ok) {
                this.currentUser = { username: data.username, role: data.role };
                errorEl.textContent = '';
                this.showDashboard();
            } else if (response.headers.get('Retry-After')) {
                const seconds = parseInt(response.headers.get('Retry-After'), 10);
                const minutes = Math.max(1, Math.ceil(seconds / 60)) || 1;
                errorEl.textContent = `Too many failed attempts. Try again in ${minutes} minute${minutes === 1 ? '' : 's'}.`;
            } else {
                errorEl.textContent = data.error || 'Login failed';
            }
        } catch (error) {
            errorEl.textContent = 'Connection error';
        }
    }

    async createAdmin() {
        const username = document.getElementById('setup-username').value;
        const password = document.getElementById('setup-password').value;
        const confirmation = document.getElementById('setup-confirm').value;
        const errorEl = document.getElementById('setup-error');

        if (password !== confirmation) {
            errorEl.textContent = 'Passwords do not match';
            return;
        }

        try {
            const response = await fetch('/api/setup', {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json'
                },
                body: JSON.stringify({ username, password }),
                credentials: 'include'
            });

            const data = await response.json();
            if (response.ok) {
                this.currentUser = { username: data.username, role: data.role };
                errorEl.textContent = '';
                this.showDashboard();
            } else if (response.status === 409) {
                // Someone else finished setup first
                this.showLogin(data.error);
            } else {
                errorEl.textContent = data.error || 'Setup failed';
            }
        } catch (error) {
            errorEl.textContent = 'Connection error';
        }
    }

    async logout() {
        // Stopping pairing needs the session, so it has to finish first
        if (this.pairingActive) {
            await this.stopZigbeePairing();
        }

        // The cookie can't be cleared from here, so until the hub confirms, the
        // session is still live and the next reload would bring it back.
        try {
            const response = await fetch('/api/auth/logout', {
                method: 'POST',
                credentials: 'include'
            });
            if (!response.ok) {
                this.showToast('Could not log out. Please try again.', 'error');
                return;
            }
        } catch (error) {
            this.showToast('Could not log out. Please try again.', 'error');
            return;
        }

        this.endSession();
        this.showLogin();
    }

    showScreen(id) {
        // Passwords shouldn't outlive the screen they were typed on
        for (const field of ['password', 'setup-password', 'setup-confirm']) {
            document.getElementById(field).value = '';
        }

        for (const screen of ['setup-screen', 'login-screen', 'dashboard-screen']) {
            document.getElementById(screen).classList.toggle('hidden', screen !== id);
        }
    }

    showSetup() {
        this.showScreen('setup-screen');
    }

    showLogin(message = '') {
        document.getElementById('login-error').textContent = message;
        this.showScreen('login-screen');
    }

    showDashboard() {
        this.showScreen('dashboard-screen');

        document.getElementById('user-name').textContent = this.currentUser.username || 'User';
        document.body.classList.toggle('is-admin', this.isAdmin());

        this.loadData();
        this.startRefresh();
    }

    startRefresh() {
        this.stopRefresh();
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

    // ==================== Data Loading ====================

    async loadData() {
        await Promise.all([
            this.loadDevices(),
            this.loadRooms(),
            this.loadAutomations()
        ]);
        // The session may have ended while those were loading
        if (this.currentUser) {
            this.render();
        }
    }

    async loadDevices() {
        try {
            const response = await this.api('/api/devices');

            if (response.ok) {
                const devices = await response.json();
                this.processDevices(devices);
            }
        } catch (error) {
            console.error('Failed to load devices:', error);
        }
    }

    async loadRooms() {
        try {
            const response = await this.api('/api/rooms');

            if (response.ok) {
                this.rooms = await response.json();
            }
        } catch (error) {
            console.error('Failed to load rooms:', error);
        }
    }

    async loadAutomations() {
        try {
            const response = await this.api('/api/automations');

            if (response.ok) {
                this.automations = await response.json();
            }
        } catch (error) {
            console.error('Failed to load automations:', error);
        }
    }

    processDevices(devices) {
        this.devices = devices.filter(device => !this.isSensor(device.type));
        this.sensors = devices.filter(device => this.isSensor(device.type));
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

    // ==================== Rendering ====================

    render() {
        this.renderStats();
        this.renderRooms();
        this.renderDevices();
        this.renderSensors();
        this.renderRoomFilter();
        this.renderAutomations();
    }

    renderStats() {
        document.getElementById('total-devices').textContent = this.devices.length;
        document.getElementById('active-devices').textContent = this.devices.filter(d => d.state?.on).length;
        document.getElementById('total-sensors').textContent = this.sensors.length;
        document.getElementById('total-rooms').textContent = this.rooms.length;
    }

    renderRooms() {
        const container = document.getElementById('rooms-grid');
        container.innerHTML = '';

        if (this.rooms.length === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">🏠</div>
                    <div class="empty-state-text">No rooms yet. Add a room to organize your devices.</div>
                </div>
            `;
            return;
        }

        this.rooms.forEach(room => {
            const roomDevices = [...this.devices, ...this.sensors].filter(d => d.room === room.name);
            const activeCount = roomDevices.filter(d => d.state?.on).length;
            const tempSensor = roomDevices.find(d => d.type?.includes('temperature'));
            const temp = tempSensor?.state?.value;

            const card = document.createElement('div');
            card.className = 'room-card';
            card.innerHTML = `
                <button class="room-card-menu admin-only" data-room-id="${this.escapeHtml(room.id)}">⋮</button>
                <div class="room-header">
                    <div class="room-name">${this.escapeHtml(room.name)}</div>
                    <div class="room-temp">${temp ? `${this.escapeHtml(temp)}°` : ''}</div>
                </div>
                <div class="room-devices">
                    ${roomDevices.slice(0, 6).map(d => `
                        <div class="room-device ${d.state?.on ? 'on' : ''}">
                            ${this.getDeviceIcon(d.type)}
                            <span>${this.escapeHtml(d.name.split(' ').slice(-1)[0])}</span>
                        </div>
                    `).join('')}
                    ${roomDevices.length > 6 ? `<div class="room-device">+${roomDevices.length - 6}</div>` : ''}
                </div>
            `;

            // Add click handler for edit button
            card.querySelector('.room-card-menu').addEventListener('click', (e) => {
                e.stopPropagation();
                this.openEditRoomModal(room);
            });

            container.appendChild(card);
        });
    }

    renderDevices() {
        const container = document.getElementById('devices-list');
        container.innerHTML = '';

        if (this.devices.length === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">💡</div>
                    <div class="empty-state-text">No devices yet. Add a device to get started.</div>
                </div>
            `;
            return;
        }

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
                        <h3>${this.escapeHtml(device.name)}</h3>
                        <div class="device-meta">${this.escapeHtml(device.room || 'No room')} • ${this.escapeHtml(device.type)}</div>
                    </div>
                </div>
                <div class="device-controls">
                    ${hasBrightness ? `
                        <div class="brightness-control">
                            <input type="range" class="brightness-slider"
                                min="0" max="100" value="${this.escapeHtml(brightness ?? 100)}"
                                data-device-id="${this.escapeHtml(device.id)}">
                            <span class="brightness-value">${this.escapeHtml(brightness ?? 100)}%</span>
                        </div>
                    ` : ''}
                    <button class="device-edit-btn admin-only" data-device-id="${this.escapeHtml(device.id)}" title="Edit">✏️</button>
                    <label class="toggle">
                        <input type="checkbox" ${isOn ? 'checked' : ''}
                            data-device-id="${this.escapeHtml(device.id)}">
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
                    card.querySelector('.brightness-value').textContent = `${e.target.value}%`;
                });
                slider.addEventListener('change', (e) => {
                    this.setBrightness(device.id, parseInt(e.target.value));
                });
            }

            const editBtn = card.querySelector('.device-edit-btn');
            editBtn.addEventListener('click', () => this.openEditDeviceModal(device));

            container.appendChild(card);
        });
    }

    renderSensors() {
        const container = document.getElementById('sensors-grid');
        container.innerHTML = '';

        if (this.sensors.length === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">📊</div>
                    <div class="empty-state-text">No sensors yet.</div>
                </div>
            `;
            return;
        }

        this.sensors.forEach(sensor => {
            const card = document.createElement('div');
            card.className = 'sensor-card';

            const value = sensor.state?.value;
            const { displayValue, unit, colorClass } = this.formatSensorValue(sensor.type, value);

            card.innerHTML = `
                <div class="sensor-header">
                    <div>
                        <div class="sensor-name">${this.escapeHtml(sensor.name)}</div>
                        <div class="sensor-room">${this.escapeHtml(sensor.room || 'No room')}</div>
                    </div>
                    <div>${this.getSensorIcon(sensor.type)}</div>
                </div>
                <div class="sensor-value ${colorClass}">${this.escapeHtml(displayValue)}${unit}</div>
                <div class="sensor-updated">Updated just now</div>
            `;

            container.appendChild(card);
        });
    }

    renderRoomFilter() {
        const select = document.getElementById('room-filter');
        const currentValue = select.value;

        select.innerHTML = '<option value="">All Rooms</option>';

        this.rooms.forEach(room => {
            const option = document.createElement('option');
            option.value = room.name;
            option.textContent = room.name;
            select.appendChild(option);
        });

        select.value = currentValue;
    }

    renderAutomations() {
        const container = document.getElementById('automations-list');
        container.innerHTML = '';

        if (this.automations.length === 0) {
            container.innerHTML = `
                <div class="empty-state">
                    <div class="empty-state-icon">⚡</div>
                    <div class="empty-state-text">No automations yet. Create an automation to automate your home.</div>
                </div>
            `;
            return;
        }

        this.automations.forEach(automation => {
            const card = document.createElement('div');
            card.className = `automation-card ${automation.enabled ? '' : 'disabled'}`;

            card.innerHTML = `
                <div class="automation-info">
                    <div class="automation-name">${this.escapeHtml(automation.name)}</div>
                    <div class="automation-trigger">${this.escapeHtml(this.formatTriggerSummary(automation.trigger || automation.triggers?.[0]))}</div>
                </div>
                <div class="automation-controls">
                    <button class="automation-edit-btn admin-only" data-automation-id="${this.escapeHtml(automation.id)}" title="Edit">✏️</button>
                    <label class="toggle">
                        <input type="checkbox" ${automation.enabled ? 'checked' : ''} ${this.isAdmin() ? '' : 'disabled'}
                            data-automation-id="${this.escapeHtml(automation.id)}">
                        <span class="toggle-slider"></span>
                    </label>
                </div>
            `;

            // Add event listeners
            const toggle = card.querySelector('.toggle input');
            toggle.addEventListener('change', (e) => {
                this.toggleAutomation(automation.id, e.target.checked);
            });

            const editBtn = card.querySelector('.automation-edit-btn');
            editBtn.addEventListener('click', () => this.openEditAutomationModal(automation));

            container.appendChild(card);
        });
    }

    escapeHtml(value) {
        const element = document.createElement('div');
        element.textContent = String(value ?? '');
        return element.innerHTML.replace(/"/g, '&quot;').replace(/'/g, '&#39;');
    }

    formatTriggerSummary(trigger) {
        if (!trigger) return 'No trigger';

        switch (trigger.type) {
            case 'device_state':
                return trigger.to_value === undefined
                    ? `When ${trigger.device_id || 'device'} ${trigger.property} changes`
                    : `When ${trigger.device_id || 'device'} ${trigger.property} becomes ${trigger.to_value}`;
            case 'time': {
                const hour = trigger.hour === -1 ? 'every hour' : trigger.hour;
                const minute = trigger.minute === -1 ? 'every minute' : String(trigger.minute).padStart(2, '0');
                return trigger.hour === -1 || trigger.minute === -1
                    ? `At ${hour}, ${minute}`
                    : `At ${hour}:${minute}`;
            }
            case 'time_interval':
                return `Every ${trigger.interval_minutes} minutes`;
            case 'sensor_threshold':
                return `When ${trigger.device_id || 'sensor'} ${trigger.property} ${trigger.compare_op} ${trigger.threshold}`;
            default:
                return trigger.type || 'Unknown trigger';
        }
    }

    // ==================== Modal Management ====================

    openModal(modalId) {
        document.getElementById('modal-overlay').classList.remove('hidden');
        document.getElementById(modalId).classList.remove('hidden');

        // Focus first input
        const firstInput = document.querySelector(`#${modalId} input:not([type="hidden"])`);
        if (firstInput) {
            setTimeout(() => firstInput.focus(), 100);
        }
    }

    closeAllModals() {
        document.getElementById('modal-overlay').classList.add('hidden');
        document.querySelectorAll('.modal').forEach(modal => {
            modal.classList.add('hidden');
        });

        // Stop pairing if active
        if (this.pairingActive) {
            this.stopZigbeePairing();
        }

        // Reset wizard states
        this.resetDeviceWizard();
        this.resetAutomationWizard();
    }

    // ==================== Room Management ====================

    openAddRoomModal() {
        document.getElementById('room-name').value = '';
        this.openModal('add-room-modal');
    }

    openEditRoomModal(room) {
        document.getElementById('edit-room-id').value = room.id;
        document.getElementById('edit-room-name').value = room.name;
        this.openModal('edit-room-modal');
    }

    async saveRoom() {
        const name = document.getElementById('room-name').value.trim();
        if (!name) {
            this.showToast('Please enter a room name', 'error');
            return;
        }

        try {
            const response = await this.api('/api/rooms', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name })
            });

            if (response.ok) {
                this.showToast('Room created', 'success');
                this.closeAllModals();
                await this.loadRooms();
                this.render();
            } else {
                const data = await response.json();
                this.showToast(data.error || 'Failed to create room', 'error');
            }
        } catch (error) {
            this.showToast('Connection error', 'error');
        }
    }

    async updateRoom() {
        const id = document.getElementById('edit-room-id').value;
        const name = document.getElementById('edit-room-name').value.trim();

        if (!name) {
            this.showToast('Please enter a room name', 'error');
            return;
        }

        try {
            const response = await this.api(`/api/rooms/${id}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name })
            });

            if (response.ok) {
                this.showToast('Room updated', 'success');
                this.closeAllModals();
                await this.loadRooms();
                this.render();
            } else {
                const data = await response.json();
                this.showToast(data.error || 'Failed to update room', 'error');
            }
        } catch (error) {
            this.showToast('Connection error', 'error');
        }
    }

    confirmDeleteRoom() {
        const id = document.getElementById('edit-room-id').value;
        const name = document.getElementById('edit-room-name').value;

        document.getElementById('delete-message').textContent =
            `Are you sure you want to delete "${name}"? Devices in this room will be unassigned.`;

        this.deleteCallback = () => this.deleteRoom(id);
        this.openModal('confirm-delete-modal');
    }

    async deleteRoom(id) {
        try {
            const response = await this.api(`/api/rooms/${id}`, {
                method: 'DELETE'
            });

            if (response.ok) {
                this.showToast('Room deleted', 'success');
                await this.loadRooms();
                this.render();
            } else {
                const data = await response.json();
                this.showToast(data.error || 'Failed to delete room', 'error');
            }
        } catch (error) {
            this.showToast('Connection error', 'error');
        }
    }

    // ==================== Device Management ====================

    openAddDeviceModal() {
        this.resetDeviceWizard();
        this.populateDeviceRoomSelect('device-room');
        this.openModal('add-device-modal');
    }

    openEditDeviceModal(device) {
        document.getElementById('edit-device-id').value = device.id;
        document.getElementById('edit-device-name').value = device.name;
        document.getElementById('edit-device-type').textContent = device.type;
        document.getElementById('edit-device-protocol').textContent = device.protocol || 'local';

        this.populateDeviceRoomSelect('edit-device-room');
        document.getElementById('edit-device-room').value = device.room || '';

        this.openModal('edit-device-modal');
    }

    populateDeviceRoomSelect(selectId) {
        const select = document.getElementById(selectId);
        select.innerHTML = '<option value="">No Room</option>';

        this.rooms.forEach(room => {
            const option = document.createElement('option');
            option.value = room.name;
            option.textContent = room.name;
            select.appendChild(option);
        });
    }

    resetDeviceWizard() {
        this.deviceWizard = {
            step: 1,
            type: null,
            name: '',
            protocol: 'local',
            room: '',
            config: {}
        };

        // Reset UI
        document.querySelectorAll('.device-type-card').forEach(c => c.classList.remove('selected'));
        document.getElementById('device-name').value = '';
        document.getElementById('device-protocol').value = 'local';
        document.getElementById('zigbee-address').value = '';
        document.getElementById('mqtt-topic').value = '';
        document.getElementById('http-url').value = '';

        this.showDeviceWizardStep(1);
        if (this.pairingActive) {
            this.stopZigbeePairing();
        }
    }

    showDeviceWizardStep(step) {
        for (let i = 1; i <= 4; i++) {
            document.getElementById(`device-step-${i}`).classList.toggle('active', i === step);
            document.getElementById(`device-step-${i}`).classList.toggle('hidden', i !== step);
        }

        // Update buttons
        document.getElementById('device-prev-btn').classList.toggle('hidden', step === 1);
        document.getElementById('device-next-btn').textContent = step === 4 ? 'Create Device' : 'Next';
    }

    deviceWizardPrev() {
        if (this.deviceWizard.step > 1) {
            this.deviceWizard.step--;
            this.showDeviceWizardStep(this.deviceWizard.step);
        }
    }

    deviceWizardNext() {
        const step = this.deviceWizard.step;

        // Validate current step
        if (step === 1) {
            if (!this.deviceWizard.type) {
                this.showToast('Please select a device type', 'error');
                return;
            }
        } else if (step === 2) {
            const name = document.getElementById('device-name').value.trim();
            if (!name) {
                this.showToast('Please enter a device name', 'error');
                return;
            }
            this.deviceWizard.name = name;
            this.deviceWizard.protocol = document.getElementById('device-protocol').value;

            // Show appropriate protocol config
            this.showProtocolConfig(this.deviceWizard.protocol);
        } else if (step === 3) {
            // Collect protocol-specific config
            this.collectProtocolConfig();
        } else if (step === 4) {
            // Create device
            this.createDevice();
            return;
        }

        // Move to next step
        this.deviceWizard.step++;
        this.showDeviceWizardStep(this.deviceWizard.step);

        // Update summary on step 4
        if (this.deviceWizard.step === 4) {
            this.deviceWizard.room = document.getElementById('device-room').value;
            this.updateDeviceSummary();
        }
    }

    showProtocolConfig(protocol) {
        document.querySelectorAll('.protocol-config').forEach(el => {
            el.classList.remove('active');
            el.classList.add('hidden');
        });

        const configEl = document.getElementById(`config-${protocol}`);
        if (configEl) {
            configEl.classList.add('active');
            configEl.classList.remove('hidden');
        }
    }

    collectProtocolConfig() {
        const protocol = this.deviceWizard.protocol;

        if (protocol === 'zigbee') {
            const address = document.getElementById('zigbee-address').value.trim();
            if (address || this.discoveredDevice) {
                this.deviceWizard.config = {
                    ieeeAddress: address || this.discoveredDevice?.ieeeAddress
                };
            }
        } else if (protocol === 'mqtt') {
            this.deviceWizard.config = {
                topic: document.getElementById('mqtt-topic').value.trim()
            };
        } else if (protocol === 'http') {
            this.deviceWizard.config = {
                url: document.getElementById('http-url').value.trim()
            };
        }
    }

    updateDeviceSummary() {
        const summary = document.getElementById('device-summary');
        summary.innerHTML = `
            <div class="device-summary-row">
                <span class="device-summary-label">Type</span>
                <span>${this.escapeHtml(this.deviceWizard.type)}</span>
            </div>
            <div class="device-summary-row">
                <span class="device-summary-label">Name</span>
                <span>${this.escapeHtml(this.deviceWizard.name)}</span>
            </div>
            <div class="device-summary-row">
                <span class="device-summary-label">Protocol</span>
                <span>${this.escapeHtml(this.deviceWizard.protocol)}</span>
            </div>
            <div class="device-summary-row">
                <span class="device-summary-label">Room</span>
                <span>${this.escapeHtml(this.deviceWizard.room || 'None')}</span>
            </div>
        `;
    }

    async createDevice() {
        const payload = {
            type: this.deviceWizard.type,
            name: this.deviceWizard.name,
            protocol: this.deviceWizard.protocol,
            room: this.deviceWizard.room || undefined,
            config: this.deviceWizard.config
        };

        try {
            const response = await this.api('/api/devices', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });

            if (response.ok) {
                this.showToast('Device created', 'success');
                this.closeAllModals();
                await this.loadDevices();
                this.render();
            } else {
                const data = await response.json();
                this.showToast(data.error || 'Failed to create device', 'error');
            }
        } catch (error) {
            this.showToast('Connection error', 'error');
        }
    }

    async updateDevice() {
        const id = document.getElementById('edit-device-id').value;
        const name = document.getElementById('edit-device-name').value.trim();
        const room = document.getElementById('edit-device-room').value;

        if (!name) {
            this.showToast('Please enter a device name', 'error');
            return;
        }

        try {
            const response = await this.api(`/api/devices/${id}/settings`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ name, room })
            });

            if (response.ok) {
                this.showToast('Device updated', 'success');
                this.closeAllModals();
                await this.loadDevices();
                this.render();
            } else {
                const data = await response.json();
                this.showToast(data.error || 'Failed to update device', 'error');
            }
        } catch (error) {
            this.showToast('Connection error', 'error');
        }
    }

    confirmDeleteDevice() {
        const id = document.getElementById('edit-device-id').value;
        const name = document.getElementById('edit-device-name').value;

        document.getElementById('delete-message').textContent =
            `Are you sure you want to delete "${name}"?`;

        this.deleteCallback = () => this.deleteDevice(id);
        this.openModal('confirm-delete-modal');
    }

    async deleteDevice(id) {
        try {
            const response = await this.api(`/api/devices/${id}`, {
                method: 'DELETE'
            });

            if (response.ok) {
                this.showToast('Device deleted', 'success');
                await this.loadDevices();
                this.render();
            } else {
                const data = await response.json();
                this.showToast(data.error || 'Failed to delete device', 'error');
            }
        } catch (error) {
            this.showToast('Connection error', 'error');
        }
    }

    // ==================== Zigbee Pairing ====================

    async startZigbeePairing() {
        try {
            const response = await this.api('/api/zigbee/permit-join', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ duration: 60 })
            });

            if (response.ok) {
                this.pairingActive = true;
                document.getElementById('start-pairing-btn').classList.add('hidden');
                document.getElementById('pairing-status').classList.remove('hidden');
                document.getElementById('discovered-device').classList.add('hidden');
                this.discoveredDevice = null;

                // Start polling for discovered devices
                this.pairingPollInterval = setInterval(() => this.pollPendingDevices(), 2000);

                // Auto-stop after 60 seconds
                setTimeout(() => {
                    if (this.pairingActive) {
                        this.stopZigbeePairing();
                    }
                }, 60000);
            } else {
                this.showToast('Failed to start pairing', 'error');
            }
        } catch (error) {
            this.showToast('Connection error', 'error');
        }
    }

    async stopZigbeePairing() {
        if (this.pairingPollInterval) {
            clearInterval(this.pairingPollInterval);
            this.pairingPollInterval = null;
        }

        this.pairingActive = false;
        document.getElementById('start-pairing-btn').classList.remove('hidden');
        document.getElementById('pairing-status').classList.add('hidden');

        try {
            await this.api('/api/zigbee/permit-join', {
                method: 'DELETE'
            });
        } catch (error) {
            console.error('Failed to stop pairing:', error);
        }
    }

    async pollPendingDevices() {
        try {
            const response = await this.api('/api/zigbee/pending-devices');

            if (response.ok) {
                const devices = await response.json();
                if (devices.length > 0) {
                    this.discoveredDevice = devices[0];
                    document.getElementById('discovered-name').textContent = this.discoveredDevice.name || 'Unknown Device';
                    document.getElementById('discovered-type').textContent = this.discoveredDevice.type || 'Unknown Type';
                    document.getElementById('discovered-device').classList.remove('hidden');
                    document.getElementById('pairing-status').classList.add('hidden');

                    // Fill in IEEE address
                    if (this.discoveredDevice.ieeeAddress) {
                        document.getElementById('zigbee-address').value = this.discoveredDevice.ieeeAddress;
                    }

                    // Stop polling
                    this.stopZigbeePairing();
                }
            }
        } catch (error) {
            console.error('Failed to poll pending devices:', error);
        }
    }

    // ==================== Automation Management ====================

    openAddAutomationModal() {
        this.resetAutomationWizard();
        this.populateTriggerDevices();
        this.populateActionDevices();
        this.populateHourSelect();
        this.openModal('add-automation-modal');
    }

    openEditAutomationModal(automation) {
        // For now, just show a toast - full edit would require more UI
        this.showToast('Edit automation coming soon', 'info');
    }

    populateTriggerDevices() {
        const allDevices = [...this.devices, ...this.sensors];

        // Trigger device select
        const triggerSelect = document.getElementById('trigger-device');
        triggerSelect.innerHTML = '';
        allDevices.forEach(d => {
            const option = document.createElement('option');
            option.value = d.id;
            option.textContent = d.name;
            triggerSelect.appendChild(option);
        });

        // Sensor select
        const sensorSelect = document.getElementById('trigger-sensor-device');
        sensorSelect.innerHTML = '';
        this.sensors.forEach(s => {
            const option = document.createElement('option');
            option.value = s.id;
            option.textContent = s.name;
            sensorSelect.appendChild(option);
        });
    }

    populateActionDevices() {
        const select = document.getElementById('action-device');
        select.innerHTML = '';
        this.devices.forEach(d => {
            const option = document.createElement('option');
            option.value = d.id;
            option.textContent = d.name;
            select.appendChild(option);
        });
    }

    populateHourSelect() {
        const select = document.getElementById('trigger-hour');
        select.innerHTML = '';
        for (let i = 0; i < 24; i++) {
            const option = document.createElement('option');
            option.value = i;
            option.textContent = `${i.toString().padStart(2, '0')}`;
            select.appendChild(option);
        }
    }

    resetAutomationWizard() {
        this.automationWizard = {
            step: 1,
            name: '',
            description: '',
            trigger: null,
            actions: []
        };

        // Reset UI
        document.getElementById('automation-name').value = '';
        document.getElementById('automation-description').value = '';
        document.getElementById('trigger-type').value = 'device_state';
        document.getElementById('actions-list').innerHTML = '';
        document.getElementById('action-form').classList.add('hidden');

        this.showAutomationWizardStep(1);
        this.showTriggerConfig('device_state');
    }

    showAutomationWizardStep(step) {
        for (let i = 1; i <= 3; i++) {
            document.getElementById(`automation-step-${i}`).classList.toggle('active', i === step);
            document.getElementById(`automation-step-${i}`).classList.toggle('hidden', i !== step);
        }

        document.getElementById('automation-prev-btn').classList.toggle('hidden', step === 1);
        document.getElementById('automation-next-btn').textContent = step === 3 ? 'Create' : 'Next';
    }

    showTriggerConfig(type) {
        document.querySelectorAll('.trigger-config').forEach(el => {
            el.classList.remove('active');
            el.classList.add('hidden');
        });

        const configId = {
            'device_state': 'trigger-device-state',
            'time': 'trigger-time',
            'time_interval': 'trigger-interval',
            'sensor_threshold': 'trigger-sensor'
        }[type];

        if (configId) {
            document.getElementById(configId).classList.add('active');
            document.getElementById(configId).classList.remove('hidden');
        }
    }

    automationWizardPrev() {
        if (this.automationWizard.step > 1) {
            this.automationWizard.step--;
            this.showAutomationWizardStep(this.automationWizard.step);
        }
    }

    automationWizardNext() {
        const step = this.automationWizard.step;

        if (step === 1) {
            const name = document.getElementById('automation-name').value.trim();
            if (!name) {
                this.showToast('Please enter an automation name', 'error');
                return;
            }
            this.automationWizard.name = name;
            this.automationWizard.description = document.getElementById('automation-description').value.trim();
        } else if (step === 2) {
            this.collectTrigger();
        } else if (step === 3) {
            if (this.automationWizard.actions.length === 0) {
                this.showToast('Please add at least one action', 'error');
                return;
            }
            this.createAutomation();
            return;
        }

        this.automationWizard.step++;
        this.showAutomationWizardStep(this.automationWizard.step);
    }

    collectTrigger() {
        const type = document.getElementById('trigger-type').value;
        let trigger = { type };

        switch (type) {
            case 'device_state':
                trigger.device_id = document.getElementById('trigger-device').value;
                trigger.property = document.getElementById('trigger-property').value;
                trigger.to_value = document.getElementById('trigger-value').value === 'true';
                break;
            case 'time':
                trigger.hour = parseInt(document.getElementById('trigger-hour').value);
                trigger.minute = parseInt(document.getElementById('trigger-minute').value);
                break;
            case 'time_interval':
                trigger.interval_minutes = parseInt(document.getElementById('trigger-interval-mins').value);
                break;
            case 'sensor_threshold':
                trigger.device_id = document.getElementById('trigger-sensor-device').value;
                trigger.property = document.getElementById('trigger-sensor-property').value;
                trigger.compare_op = document.getElementById('trigger-operator').value;
                trigger.threshold = parseFloat(document.getElementById('trigger-threshold').value);
                break;
        }

        this.automationWizard.trigger = trigger;
    }

    addAction() {
        const deviceId = document.getElementById('action-device').value;
        const deviceName = document.getElementById('action-device').selectedOptions[0]?.textContent || deviceId;
        const property = document.getElementById('action-property').value;
        const value = document.getElementById('action-value').value === 'true';

        const action = { deviceId, property, value };
        this.automationWizard.actions.push(action);

        // Add to UI
        const actionsList = document.getElementById('actions-list');
        const actionEl = document.createElement('div');
        actionEl.className = 'action-item';
        actionEl.innerHTML = `
            <span class="action-item-text">Set ${this.escapeHtml(deviceName)} ${this.escapeHtml(property)} to ${value ? 'On' : 'Off'}</span>
            <button class="action-remove-btn">×</button>
        `;

        actionEl.querySelector('.action-remove-btn').addEventListener('click', () => {
            const index = this.automationWizard.actions.indexOf(action);
            if (index >= 0) {
                this.automationWizard.actions.splice(index, 1);
            }
            actionEl.remove();
        });

        actionsList.appendChild(actionEl);
        document.getElementById('action-form').classList.add('hidden');
    }

    async createAutomation() {
        const payload = {
            name: this.automationWizard.name,
            description: this.automationWizard.description,
            enabled: true,
            trigger: this.automationWizard.trigger,
            actions: this.automationWizard.actions
        };

        try {
            const response = await this.api('/api/automations', {
                method: 'POST',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify(payload)
            });

            if (response.ok) {
                this.showToast('Automation created', 'success');
                this.closeAllModals();
                await this.loadAutomations();
                this.render();
            } else {
                const data = await response.json();
                this.showToast(data.error || 'Failed to create automation', 'error');
            }
        } catch (error) {
            this.showToast('Connection error', 'error');
        }
    }

    async toggleAutomation(id, enabled) {
        try {
            const response = await this.api(`/api/automations/${id}/enable`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ enabled })
            });

            if (response.ok) {
                const automation = this.automations.find(a => a.id === id);
                if (automation) {
                    automation.enabled = enabled;
                }
                this.showToast(enabled ? 'Automation enabled' : 'Automation disabled', 'success');
            } else {
                this.showToast('Failed to update automation', 'error');
            }
        } catch (error) {
            this.showToast('Connection error', 'error');
        }
    }

    // ==================== Device Control ====================

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
            const response = await this.api(`/api/devices/${deviceId}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ on })
            });

            if (response.ok) {
                await this.applyDeviceResponse(deviceId, response);
                this.showToast(`${on ? 'Turned on' : 'Turned off'}`, 'success');
            } else {
                this.showToast('Failed to update device', 'error');
                this.renderDevices();  // Put the control back to the real state
            }
        } catch (error) {
            console.error('Toggle error:', error);
            this.showToast('Connection error', 'error');
            this.renderDevices();
        }
    }

    // The server's state can change more than was asked: setting brightness also sets on.
    async applyDeviceResponse(deviceId, response) {
        const result = await response.json();
        const device = this.devices.find(d => d.id === deviceId);
        if (device && result.state) {
            device.state = result.state;
            this.renderRooms();
            this.renderDevices();
        }
    }

    async setBrightness(deviceId, brightness) {
        try {
            const response = await this.api(`/api/devices/${deviceId}`, {
                method: 'PUT',
                headers: { 'Content-Type': 'application/json' },
                body: JSON.stringify({ brightness })
            });

            if (response.ok) {
                await this.applyDeviceResponse(deviceId, response);
            } else {
                this.showToast('Failed to update device', 'error');
                this.renderDevices();  // Put the control back to the real state
            }
        } catch (error) {
            console.error('Brightness error:', error);
            this.showToast('Connection error', 'error');
            this.renderDevices();
        }
    }

    // ==================== Helpers ====================

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
            return { displayValue: Number(value).toFixed(1), unit: '°F', colorClass: 'temperature' };
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
        document.querySelectorAll('.tab').forEach(tab => {
            tab.classList.toggle('active', tab.dataset.tab === tabName);
        });

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
