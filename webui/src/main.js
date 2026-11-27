import Vue from 'vue'

import Vuex from "vuex";
Vue.use(Vuex);

import VueRouter from 'vue-router'
Vue.use(VueRouter)

import Axios from 'axios'

import 'bootstrap/dist/css/bootstrap.css'
import 'bootstrap-vue/dist/bootstrap-vue.css'

import App from './app.vue'
import Home from './home.vue'
import Settings from "./settings.vue";
import FirmwareUpdate from "./firmwareupdate.vue";
import Login from './login.vue'
import About from './about.vue'

const moduleLogin = {
  namespaced: true,
  state: () => ({
    isLoggedIn: localStorage.getItem("hb-rf-eth-pw") != null,
    token: localStorage.getItem("hb-rf-eth-pw")
  }),
  mutations: {
    login(state, token) {
      localStorage.setItem("hb-rf-eth-pw", token);
      state.token = token;
      state.isLoggedIn = true;
    },
    logout(state) {
      state.isLoggedIn = false;
      localStorage.removeItem("hb-rf-eth-pw");
      state.token = "";
    },
  },
  actions: {
    tryLogin(context, password) {
      return new Promise((resolve, reject) => {
        Axios
          .post("/login.json", { password: password })
          .then(
            response => {
              if (response.data.isAuthenticated) {
                context.commit("login", response.data.token);

                resolve();
              } else {
                reject();
              }
            },
            () => {
              reject();
            })
      });
    },
    logout(context) {
      context.commit("logout");
    }
  }
};

const moduleSysInfo = {
  namespaced: true,
  state: () => ({
    serial: "",
    currentVersion: "",
    latestVersion: "",
    rawUartRemoteAddress: "",
    memoryUsage: 0.0,
    cpuUsage: 0.0,
    radioModuleType: "",
    radioModuleSerial: "",
    radioModuleBidCosRadioMAC: "",
    radioModuleHmIPRadioMAC: "",
    radioModuleSGTIN: "",
    ccuRestartRequired: false,
    resetReason: ""
  }),
  mutations: {
    sysInfo(state, newState) {
      state.serial = newState.serial;
      state.currentVersion = newState.currentVersion;
      state.latestVersion = newState.latestVersion;
      state.rawUartRemoteAddress = newState.rawUartRemoteAddress;
      state.memoryUsage = newState.memoryUsage;
      state.cpuUsage = newState.cpuUsage;
      state.radioModuleType = newState.radioModuleType;
      state.radioModuleSerial = newState.radioModuleSerial;
      state.radioModuleBidCosRadioMAC = newState.radioModuleBidCosRadioMAC;
      state.radioModuleHmIPRadioMAC = newState.radioModuleHmIPRadioMAC;
      state.radioModuleSGTIN = newState.radioModuleSGTIN;
      state.ccuRestartRequired = newState.ccuRestartRequired;
      state.resetReason = newState.resetReason;
    },
  },
  actions: {
    update(context) {
      return new Promise((resolve, reject) => {
        Axios
          .get("/sysinfo.json")
          .then(
            response => {
              context.commit("sysInfo", response.data.sysInfo);
              resolve();
            }
            ,
            () => {
              reject();
            });
      });
    }
  }
};

const moduleSettings = {
  namespaced: true,
  state: () => ({
    hostname: "",
    useDHCP: true,
    localIP: "",
    netmask: "",
    gateway: "",
    dns1: "",
    dns2: "",
    timesource: 0,
    dcfOffset: 0,
    gpsBaudrate: 9600,
    ntpServer: "",
    ledBrightness: 100,
    mqttEnabled: false,
    mqttServer: "",
    mqttPort: 1883,
    mqttUser: "",
    mqttPasswordSet: false,
    mqttTopicPrefix: "",
    mqttInterval: 60,
    mqttHaDiscovery: true,
    mqttCommands: true,
    networkWatchdog: false,
    networkWatchdogTimeout: 10,
    updateRepo: "alexreinert/HB-RF-ETH",
    updateInterval: 8,
    updateAuto: false,
    restartRequired: false,
  }),
  mutations: {
    settings(state, value) {
      state.hostname = value.hostname;
      state.useDHCP = value.useDHCP;
      state.localIP = value.localIP;
      state.netmask = value.netmask;
      state.gateway = value.gateway;
      state.dns1 = value.dns1;
      state.dns2 = value.dns2;
      state.timesource = value.timesource;
      state.dcfOffset = value.dcfOffset;
      state.gpsBaudrate = value.gpsBaudrate;
      state.ntpServer = value.ntpServer;
      state.ledBrightness = value.ledBrightness;
      state.mqttEnabled = value.mqttEnabled;
      state.mqttServer = value.mqttServer;
      state.mqttPort = value.mqttPort;
      state.mqttUser = value.mqttUser;
      state.mqttPasswordSet = value.mqttPasswordSet;
      state.mqttTopicPrefix = value.mqttTopicPrefix;
      state.mqttInterval = value.mqttInterval;
      state.mqttHaDiscovery = value.mqttHaDiscovery;
      state.mqttCommands = value.mqttCommands;
      state.networkWatchdog = value.networkWatchdog;
      state.networkWatchdogTimeout = value.networkWatchdogTimeout;
      state.updateRepo = value.updateRepo;
      state.updateInterval = value.updateInterval;
      state.updateAuto = value.updateAuto;
    },
    restartRequired(state, value) {
      state.restartRequired = value;
    },
  },
  actions: {
    load(context) {
      return new Promise((resolve, reject) => {
        Axios
          .get("/settings.json")
          .then(
            response => {
              context.commit("settings", response.data.settings);
              resolve();
            },
            () => { reject(); });
      });
    },
    save(context, settings) {
      return new Promise((resolve, reject) => {
        Axios
          .post("/settings.json", settings)
          .then(
            response => {
              context.commit("settings", response.data.settings);
              if (response.data.restartRequired) {
                context.commit("restartRequired", true);
              }
              resolve();
            },
            () => { reject(); });
      });
    }
  }
};

const moduleSystem = {
  namespaced: true,
  state: () => ({
    restarting: false,
  }),
  mutations: {
    restarting(state, value) {
      state.restarting = value;
    },
  },
  actions: {
    // restarts the board and reloads the page once it answers again
    restart(context) {
      context.commit("restarting", true);
      return Axios.post("/api/restart").then(() => {
        var waitUntilBack = () => {
          setTimeout(() => {
            Axios.get("/sysinfo.json", { timeout: 2000 }).then(
              () => { window.location.reload(); },
              () => { waitUntilBack(); });
          }, 3000);
        };
        waitUntilBack();
      }, (err) => {
        context.commit("restarting", false);
        return Promise.reject(err);
      });
    }
  }
};

const moduleOnlineUpdate = {
  namespaced: true,
  state: () => ({
    state: "idle",
    repo: "",
    installedVersion: "",
    latestVersion: "",
    available: false,
    installable: false,
    releaseUrl: "",
    progress: 0,
    error: "",
  }),
  mutations: {
    update(state, value) {
      Object.assign(state, value);
    },
  },
  actions: {
    load(context) {
      return Axios.get("/api/update").then(response => context.commit("update", response.data.update));
    },
    check(context) {
      return Axios.post("/api/update/check").then(response => context.commit("update", response.data.update));
    },
    install(context, force) {
      return Axios.post("/api/update/install", { force: !!force }).then(response => {
        context.commit("update", response.data.update);
        return response.data.accepted;
      });
    }
  }
};

const moduleFirmwareUpdate = {
  namespaced: true,
  state: () => ({
    progress: 0,
  }),
  mutations: {
    progress(state, value) {
      state.progress = value;
    },
  },
  actions: {
    update(context, file) {
      return new Promise((resolve, reject) => {
        context.commit("progress", 0);

        var form = new FormData();
        form.append("file", file, file.name);
        form.append("upload_file", true);

        Axios
          .post("/ota_update", form, {
            headers: {
              'Content-Type': 'multipart/form-data'
            },
            onUploadProgress: event => {
              if (event.lengthComputable) {
                context.commit("progress", Math.ceil((event.loaded || event.position) / event.total * 100));
              }
            }
          })
          .then(
            response => {
              context.commit("progress", 0);
              resolve();
            },
            (err) => {
              reject();
            });
      });
    }
  }
};

const store = new Vuex.Store({
  modules: {
    login: moduleLogin,
    sysInfo: moduleSysInfo,
    settings: moduleSettings,
    firmwareUpdate: moduleFirmwareUpdate,
    system: moduleSystem,
    onlineUpdate: moduleOnlineUpdate,
  }
});

const router = new VueRouter({
  mode: "history",
  routes: [
    { path: '/', component: Home },
    { path: '/settings', component: Settings, meta: { requiresAuth: true } },
    { path: '/firmware', component: FirmwareUpdate, meta: { requiresAuth: true } },
    { path: '/about', component: About },
    { path: '/login', component: Login },
  ]
});

router.beforeEach((to, from, next) => {
  if (to.matched.some(r => r.meta.requiresAuth)) {
    if (!store.state.login.isLoggedIn) {
      next({
        path: '/login',
        query: { redirect: to.fullPath }
      })
      return;
    }
  }

  next()
});

Axios.interceptors.request.use(
  request => {
    if (store.state.login.isLoggedIn) {
      request.headers['Authorization'] = 'Token ' + store.state.login.token;
    }
    return request;
  },
  error => { Promise.reject(error) }
);

Axios.interceptors.response.use(
  response => { return response; },
  error => {
    if (error.response && error.response.status == 401) {
      store.dispatch("login/logout");
      router.go();
    }

    return Promise.reject(error);
  }
);

new Vue({
  el: '#app',
  store,
  router,
  render: h => h(App)
});
