<template>
  <b-card
    :header="$t('title')"
    header-tag="h6"
    header-bg-variant="secondary"
    header-text-variant="white"
    class="mb-3"
  >
    <b-form @submit.stop.prevent>
      <b-form-group :label="$t('changePassword')" label-cols-sm="4">
        <b-form-input
          type="password"
          v-model="$v.adminPassword.$model"
          :state="validateState('adminPassword')"
        ></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('repeatPassword')" label-cols-sm="4">
        <b-form-input
          type="password"
          v-model="$v.adminPasswordRepeat.$model"
          :state="validateState('adminPasswordRepeat')"
        ></b-form-input>
      </b-form-group>
      <hr />
      <b-form-group :label="$t('hostname')" label-cols-sm="4">
        <b-form-input
          type="text"
          v-model="$v.hostname.$model"
          trim
          :state="validateState('hostname')"
        ></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('dhcp')" label-cols-sm="4">
        <b-form-radio-group buttons v-model="useDHCP" required>
          <b-form-radio :value="true">{{ $t('enabled') }}</b-form-radio>
          <b-form-radio :value="false">{{ $t('disabled') }}</b-form-radio>
        </b-form-radio-group>
      </b-form-group>
      <b-form-group :label="$t('ipAddress')" label-cols-sm="4" v-if="!useDHCP">
        <b-form-input
          type="text"
          v-model="$v.localIP.$model"
          trim
          :state="validateState('localIP')"
        ></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('netmask')" label-cols-sm="4" v-if="!useDHCP">
        <b-form-input
          type="text"
          v-model="$v.netmask.$model"
          trim
          :state="validateState('netmask')"
        ></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('gateway')" label-cols-sm="4" v-if="!useDHCP">
        <b-form-input
          type="text"
          v-model="$v.gateway.$model"
          trim
          :state="validateState('gateway')"
        ></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('dns1')" label-cols-sm="4" v-if="!useDHCP">
        <b-form-input type="text" v-model="$v.dns1.$model" trim :state="validateState('dns1')"></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('dns2')" label-cols-sm="4" v-if="!useDHCP">
        <b-form-input type="text" v-model="$v.dns2.$model" trim :state="validateState('dns2')"></b-form-input>
      </b-form-group>
      <hr />
      <b-form-group :label="$t('timesource')" label-cols-sm="4">
        <b-form-radio-group buttons v-model="timesource" required>
          <b-form-radio :value="0">{{ $t('ntp') }}</b-form-radio>
          <b-form-radio :value="1">{{ $t('dcf') }}</b-form-radio>
          <b-form-radio :value="2">{{ $t('gps') }}</b-form-radio>
        </b-form-radio-group>
      </b-form-group>
      <b-form-group :label="$t('ntpServer')" label-cols-sm="4" v-if="isNtpActivated">
        <b-form-input
          type="text"
          v-model="$v.ntpServer.$model"
          trim
          :state="validateState('ntpServer')"
        ></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('dcfOffset')" label-cols-sm="4" v-if="isDcfActivated">
        <b-input-group append="µs">
          <b-form-input
            type="number"
            v-model.number="$v.dcfOffset.$model"
            min="0"
            :state="validateState('dcfOffset')"
          ></b-form-input>
        </b-input-group>
      </b-form-group>
      <b-form-group :label="$t('gpsBaudrate')" label-cols-sm="4" v-if="isGpsActivated">
        <b-form-select v-model.number="gpsBaudrate">
          <b-form-select-option :value="4800">4800</b-form-select-option>
          <b-form-select-option :value="9600">9600</b-form-select-option>
          <b-form-select-option :value="19200">19200</b-form-select-option>
          <b-form-select-option :value="38400">38400</b-form-select-option>
          <b-form-select-option :value="57600">57600</b-form-select-option>
          <b-form-select-option :value="115200">115200</b-form-select-option>
        </b-form-select>
      </b-form-group>
      <hr />
      <b-form-group :label="$t('ledBrightness')" label-cols-sm="4" :description="$t('ledBrightnessHint')">
        <b-input-group append="%">
          <b-form-input
            type="number"
            v-model.number="$v.ledBrightness.$model"
            min="0"
            max="100"
            step="5"
            :state="validateState('ledBrightness')"
          ></b-form-input>
        </b-input-group>
      </b-form-group>
      <hr />
      <b-form-group :label="$t('mqtt')" label-cols-sm="4">
        <b-form-radio-group buttons v-model="mqttEnabled" required>
          <b-form-radio :value="true">{{ $t('enabled') }}</b-form-radio>
          <b-form-radio :value="false">{{ $t('disabled') }}</b-form-radio>
        </b-form-radio-group>
      </b-form-group>
      <b-form-group :label="$t('mqttServer')" label-cols-sm="4" v-if="mqttEnabled">
        <b-form-input
          type="text"
          v-model="$v.mqttServer.$model"
          trim
          :state="validateState('mqttServer')"
        ></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('mqttPort')" label-cols-sm="4" v-if="mqttEnabled">
        <b-form-input
          type="number"
          v-model.number="$v.mqttPort.$model"
          min="1"
          max="65535"
          :state="validateState('mqttPort')"
        ></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('mqttUser')" label-cols-sm="4" v-if="mqttEnabled">
        <b-form-input
          type="text"
          v-model="$v.mqttUser.$model"
          trim
          autocomplete="off"
          :state="validateState('mqttUser')"
        ></b-form-input>
      </b-form-group>
      <b-form-group
        :label="$t('mqttPassword')"
        :description="mqttPasswordSet ? $t('mqttPasswordKeep') : ''"
        label-cols-sm="4"
        v-if="mqttEnabled"
      >
        <b-form-input
          type="password"
          v-model="$v.mqttPassword.$model"
          autocomplete="new-password"
          :placeholder="mqttPasswordSet ? '••••••••' : ''"
          :state="validateState('mqttPassword')"
        ></b-form-input>
      </b-form-group>
      <b-form-group
        :label="$t('mqttTopicPrefix')"
        :description="$t('mqttTopicPrefixHint')"
        label-cols-sm="4"
        v-if="mqttEnabled"
      >
        <b-form-input
          type="text"
          v-model="$v.mqttTopicPrefix.$model"
          trim
          placeholder="hb-rf-eth/<serial>"
          :state="validateState('mqttTopicPrefix')"
        ></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('mqttInterval')" label-cols-sm="4" v-if="mqttEnabled">
        <b-input-group append="s">
          <b-form-input
            type="number"
            v-model.number="$v.mqttInterval.$model"
            min="5"
            max="3600"
            :state="validateState('mqttInterval')"
          ></b-form-input>
        </b-input-group>
      </b-form-group>
      <b-form-group :label="$t('mqttHaDiscovery')" label-cols-sm="4" v-if="mqttEnabled">
        <b-form-radio-group buttons v-model="mqttHaDiscovery" required>
          <b-form-radio :value="true">{{ $t('enabled') }}</b-form-radio>
          <b-form-radio :value="false">{{ $t('disabled') }}</b-form-radio>
        </b-form-radio-group>
      </b-form-group>
      <b-form-group
        :label="$t('mqttCommands')"
        label-cols-sm="4"
        v-if="mqttEnabled"
        :description="$t('mqttCommandsHint')"
      >
        <b-form-radio-group buttons v-model="mqttCommands" required>
          <b-form-radio :value="true">{{ $t('enabled') }}</b-form-radio>
          <b-form-radio :value="false">{{ $t('disabled') }}</b-form-radio>
        </b-form-radio-group>
      </b-form-group>
      <hr />
      <b-form-group :label="$t('networkWatchdog')" label-cols-sm="4" :description="$t('networkWatchdogHint')">
        <b-form-radio-group buttons v-model="networkWatchdog" required>
          <b-form-radio :value="true">{{ $t('enabled') }}</b-form-radio>
          <b-form-radio :value="false">{{ $t('disabled') }}</b-form-radio>
        </b-form-radio-group>
      </b-form-group>
      <b-form-group :label="$t('networkWatchdogTimeout')" label-cols-sm="4" v-if="networkWatchdog">
        <b-input-group :append="$t('minutes')">
          <b-form-input
            type="number"
            v-model.number="$v.networkWatchdogTimeout.$model"
            min="2"
            max="1440"
            :state="validateState('networkWatchdogTimeout')"
          ></b-form-input>
        </b-input-group>
      </b-form-group>
      <hr />
      <b-form-group
        :label="$t('updateRepo')"
        label-cols-sm="4"
        :description="updateSource == 'original' ? $t('updateRepoOriginalHint') : $t('updateRepoHint')"
      >
        <b-form-select v-model="updateSource" @change="updateSourceChanged">
          <b-form-select-option value="original">{{ $t('updateRepoOriginal') }} ({{ originalRepo }})</b-form-select-option>
          <b-form-select-option value="custom">{{ $t('updateRepoCustom') }}</b-form-select-option>
        </b-form-select>
        <b-form-input
          v-if="updateSource == 'custom'"
          class="mt-2"
          type="text"
          v-model="$v.updateRepo.$model"
          trim
          placeholder="KrX3D/HB-RF-ETH"
          :state="validateState('updateRepo')"
          @input="rememberCustomRepo"
        ></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('updateInterval')" label-cols-sm="4">
        <b-input-group :append="$t('hours')">
          <b-form-input
            type="number"
            v-model.number="$v.updateInterval.$model"
            min="1"
            max="168"
            :state="validateState('updateInterval')"
          ></b-form-input>
        </b-input-group>
      </b-form-group>
      <b-form-group :label="$t('updateAuto')" label-cols-sm="4" :description="$t('updateAutoHint')">
        <b-form-radio-group buttons v-model="updateAuto" required>
          <b-form-radio :value="true">{{ $t('enabled') }}</b-form-radio>
          <b-form-radio :value="false">{{ $t('disabled') }}</b-form-radio>
        </b-form-radio-group>
      </b-form-group>

      <b-alert
        variant="success"
        :show="showSuccess"
        dismissible
        fade
        @dismissed="showSuccess=null"
      >{{ $t("saveSuccess") }}</b-alert>
      <b-alert variant="warning" :show="$store.state.settings.restartRequired">
        {{ $t("restartRequired") }}
        <b-button
          size="sm"
          variant="warning"
          class="ml-2"
          :disabled="$store.state.system.restarting"
          @click="restartClick"
        >{{ $t("restartNow") }}</b-button>
      </b-alert>
      <b-alert
        variant="danger"
        :show="showError"
        dismissible
        fade
        @dismissed="showError=null"
      >{{ $t("saveError") }}</b-alert>

      <b-form-group label-cols-sm="9">
        <b-button
          variant="primary"
          block
          @click="saveSettingsClick"
          :disabled="$v.$anyError"
        >{{ $t('save') }}</b-button>
      </b-form-group>
    </b-form>
  </b-card>
</template>

<script>
import Vue from "vue";

import Vuelidate from "vuelidate";
Vue.use(Vuelidate);
import {
  required,
  requiredIf,
  requiredUnless,
  minLength,
  maxLength,
  numeric,
  ipAddress,
  sameAs,
  integer,
  between,
  helpers
} from "vuelidate/lib/validators";

const hostname = helpers.regex('hostname', /^[a-zA-Z0-9_-]{1,63}$/)
const githubRepo = helpers.regex('githubRepo', /^[A-Za-z0-9_.-]+\/[A-Za-z0-9_.-]+$/)
const mqttTopic = helpers.regex('mqttTopic', /^[^#+\s]*$/)
const domainname = helpers.regex('domainname', /^([a-zA-Z0-9_-]{1,63}\.)*[a-zA-Z0-9_-]{1,63}$/)

import VueI18n from "vue-i18n";
Vue.use(VueI18n);

import { LayoutPlugin } from "bootstrap-vue/esm/components/layout";
Vue.use(LayoutPlugin);

import { FormPlugin } from "bootstrap-vue/esm/components/form";
Vue.use(FormPlugin);

import { FormGroupPlugin } from "bootstrap-vue/esm/components/form-group";
Vue.use(FormGroupPlugin);

import { FormInputPlugin } from "bootstrap-vue/esm/components/form-input";
Vue.use(FormInputPlugin);

import { FormRadioPlugin } from "bootstrap-vue/esm/components/form-radio";
Vue.use(FormRadioPlugin);

import { FormSelectPlugin } from "bootstrap-vue/esm/components/form-select";
Vue.use(FormSelectPlugin);

const ORIGINAL_REPO = "alexreinert/HB-RF-ETH";
const CUSTOM_REPO_KEY = "hb-rf-eth-custom-repo";

import { InputGroupPlugin } from "bootstrap-vue/esm/components/input-group";
Vue.use(InputGroupPlugin);

import { ButtonPlugin } from "bootstrap-vue/esm/components/button";
Vue.use(ButtonPlugin);

import { CardPlugin } from "bootstrap-vue/esm/components/card";
Vue.use(CardPlugin);

import { AlertPlugin } from "bootstrap-vue/esm/components/alert";
Vue.use(AlertPlugin);

export default {
  name: "Settings",
  data() {
    return {
      adminPassword: "",
      adminPasswordRepeat: "",
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
      mqttPassword: "",
      mqttPasswordSet: false,
      mqttTopicPrefix: "",
      mqttInterval: 60,
      mqttHaDiscovery: true,
      mqttCommands: true,
      networkWatchdog: false,
      networkWatchdogTimeout: 10,
      updateRepo: "",
      updateSource: "original",
      originalRepo: ORIGINAL_REPO,
      updateInterval: 8,
      updateAuto: false,

      showSuccess: null,
      showError: null
    };
  },
  created() {
    this.hostname = this.$store.state.settings.hostname;
    this.useDHCP = this.$store.state.settings.useDHCP;
    this.localIP = this.$store.state.settings.localIP;
    this.netmask = this.$store.state.settings.netmask;
    this.gateway = this.$store.state.settings.gateway;
    this.dns1 = this.$store.state.settings.dns1;
    this.dns2 = this.$store.state.settings.dns2;
    this.timesource = this.$store.state.settings.timesource;
    this.gpsBaudrate = this.$store.state.settings.gpsBaudrate;
    this.ntpServer = this.$store.state.settings.ntpServer;
    this.ledBrightness = this.$store.state.settings.ledBrightness;
    this.applyMqttSettings(this.$store.state.settings);

    this.unwatch = this.$store.watch(
      (state, getters) => {
        return state.settings;
      },
      (value, oldValue) => {
        this.hostname = value.hostname;
        this.useDHCP = value.useDHCP;
        this.localIP = value.localIP;
        this.netmask = value.netmask;
        this.gateway = value.gateway;
        this.dns1 = value.dns1;
        this.dns2 = value.dns2;
        this.timesource = value.timesource;
        this.gpsBaudrate = value.gpsBaudrate;
        this.ntpServer = value.ntpServer;
        this.ledBrightness = value.ledBrightness;
        this.applyMqttSettings(value);
      },
      { deep: true }
    );
  },
  beforeDestroy() {
    this.unwatch();
  },
  computed: {
    isNtpActivated: function() {
      return this.timesource == 0;
    },
    isDcfActivated: function() {
      return this.timesource == 1;
    },
    isGpsActivated: function() {
      return this.timesource == 2;
    }
  },
  validations: {
    adminPassword: {
      minLength: minLength(5),
      maxLength: maxLength(32)
    },
    adminPasswordRepeat: {
      sameAsPassword: sameAs("adminPassword")
    },
    hostname: {
      required,
      hostname,
      maxLength: maxLength(32)
    },
    localIP: {
      required: requiredUnless("useDHCP"),
      ipAddress
    },
    netmask: {
      required: requiredUnless("useDHCP"),
      ipAddress
    },
    gateway: {
      required: requiredUnless("useDHCP"),
      ipAddress
    },
    dns1: {
      required: requiredUnless("useDHCP"),
      ipAddress
    },
    dns2: {
      ipAddress
    },
    ntpServer: {
      required: requiredIf("isNtpActived"),
      domainname,
      maxLength: maxLength(64)
    },
    dcfOffset: {
      required: requiredIf("isDcfActived"),
      numeric
    },
    mqttServer: {
      required: requiredIf("mqttEnabled"),
      domainname,
      maxLength: maxLength(64)
    },
    mqttPort: {
      required: requiredIf("mqttEnabled"),
      integer,
      between: between(1, 65535)
    },
    mqttUser: {
      maxLength: maxLength(64)
    },
    mqttPassword: {
      maxLength: maxLength(64)
    },
    mqttTopicPrefix: {
      mqttTopic,
      maxLength: maxLength(64)
    },
    mqttInterval: {
      required: requiredIf("mqttEnabled"),
      integer,
      between: between(5, 3600)
    },
    ledBrightness: {
      required,
      integer,
      between: between(0, 100)
    },
    networkWatchdogTimeout: {
      required: requiredIf("networkWatchdog"),
      integer,
      between: between(2, 1440)
    },
    updateRepo: {
      required,
      githubRepo,
      maxLength: maxLength(64)
    },
    updateInterval: {
      required,
      integer,
      between: between(1, 168)
    }
  },
  mounted() {
    this.$store.dispatch("settings/load");
  },
  methods: {
    // the original repository is always in the list; the last own repository
    // is kept in the browser so switching back and forth does not lose it
    updateSourceChanged(source) {
      if (source == "original") {
        this.updateRepo = ORIGINAL_REPO;
      } else {
        this.updateRepo = localStorage.getItem(CUSTOM_REPO_KEY) || "";
        this.$v.updateRepo.$touch();
      }
    },
    rememberCustomRepo(value) {
      if (value) localStorage.setItem(CUSTOM_REPO_KEY, value);
    },
    applyMqttSettings(value) {
      this.mqttEnabled = value.mqttEnabled;
      this.mqttServer = value.mqttServer;
      this.mqttPort = value.mqttPort;
      this.mqttUser = value.mqttUser;
      this.mqttPasswordSet = value.mqttPasswordSet;
      this.mqttTopicPrefix = value.mqttTopicPrefix;
      this.mqttInterval = value.mqttInterval;
      this.mqttHaDiscovery = value.mqttHaDiscovery;
      this.mqttCommands = value.mqttCommands;
      this.networkWatchdog = value.networkWatchdog;
      this.networkWatchdogTimeout = value.networkWatchdogTimeout;
      this.updateRepo = value.updateRepo;
      this.updateSource = (value.updateRepo || "").toLowerCase() == ORIGINAL_REPO.toLowerCase() ? "original" : "custom";
      this.updateInterval = value.updateInterval;
      this.updateAuto = value.updateAuto;
    },
    restartClick() {
      if (window.confirm(this.$t("restartConfirm"))) {
        this.$store.dispatch("system/restart").catch(() => {
          this.showError = true;
        });
      }
    },
    validateState(name) {
      const { $dirty, $error } = this.$v[name];
      return $dirty && $error ? false : null;
    },
    saveSettingsClick(event) {
      var self = this;

      self.$v.$touch();
      if (self.$v.$anyError) return;

      this.showError = false;
      this.showSuccess = false;

      this.$store
        .dispatch("settings/save", {
          adminPassword: self.adminPassword,
          hostname: self.hostname,
          useDHCP: self.useDHCP,
          localIP: self.localIP,
          netmask: self.netmask,
          gateway: self.gateway,
          dns1: self.dns1,
          dns2: self.dns2,
          timesource: self.timesource,
          dcfOffset: self.dcfOffset,
          gpsBaudrate: self.gpsBaudrate,
          ntpServer: self.ntpServer,
          ledBrightness: self.ledBrightness,
          mqttEnabled: self.mqttEnabled,
          mqttServer: self.mqttServer,
          mqttPort: self.mqttPort,
          mqttUser: self.mqttUser,
          mqttPassword: self.mqttPassword,
          mqttTopicPrefix: self.mqttTopicPrefix,
          mqttInterval: self.mqttInterval,
          mqttHaDiscovery: self.mqttHaDiscovery,
          mqttCommands: self.mqttCommands,
          networkWatchdog: self.networkWatchdog,
          networkWatchdogTimeout: self.networkWatchdogTimeout,
          updateRepo: self.updateRepo,
          updateInterval: self.updateInterval,
          updateAuto: self.updateAuto
        })
        .then(
          () => {
            self.showSuccess = true;
            // stored now; the field shows the mask instead of the plain text
            self.mqttPassword = "";
            self.$v.mqttPassword.$reset();
          },
          () => {
            self.showError = true;
          }
        );
    }
  },
  i18n: {
    locale: navigator.language,
    fallbackLocale: "en",
    messages: {
      de: {
        title: "Einstellungen",
        changePassword: "Passwort ändern",
        repeatPassword: "Passwort wiederholen",
        hostname: "Hostname",
        dhcp: "DHCP",
        enabled: "Aktiv",
        disabled: "Deaktiviert",
        ipAddress: "IP Adresse",
        netmask: "Netzmaske",
        gateway: "Gateway",
        dns1: "Primärer DNS Server",
        dns2: "Sekundärer DNS Server",
        timesource: "Zeitquelle",
        ntp: "NTP",
        dcf: "DCF",
        gps: "GPS",
        ntpServer: "NTP Server",
        dcfOffset: "DCF Versatz",
        gpsBaudrate: "GPS Baudrate",
        ledBrightness: "LED Helligkeit",
        mqtt: "MQTT",
        mqttServer: "MQTT Broker",
        mqttPort: "MQTT Port",
        mqttUser: "MQTT Benutzer",
        mqttPassword: "MQTT Passwort",
        mqttPasswordKeep: "Leer lassen, um das gespeicherte Passwort beizubehalten.",
        mqttTopicPrefix: "MQTT Topic Präfix",
        mqttTopicPrefixHint: "Leer lassen für hb-rf-eth/<Seriennummer>.",
        mqttInterval: "Sendeintervall",
        mqttHaDiscovery: "Home Assistant Discovery",
        mqttCommands: "MQTT Befehle",
        mqttCommandsHint: "Neustart, LED Helligkeit, Netzwerk-Watchdog und Firmware Update über MQTT / Home Assistant steuern.",
        ledBrightnessHint: "Wird sofort übernommen.",
        networkWatchdog: "Netzwerk-Watchdog",
        networkWatchdogHint:
          "Startet die Platine neu, wenn Netzwerkverbindung, IP Adresse oder Gateway (Ping) für die eingestellte Zeit fehlen. Wiederholte Neustarts warten jeweils doppelt so lange (max. 24 Stunden).",
        networkWatchdogTimeout: "Neustart nach",
        minutes: "Minuten",
        hours: "Stunden",
        updateRepo: "Update Quelle (GitHub)",
        updateRepoOriginal: "Original",
        updateRepoCustom: "Eigenes Repository...",
        updateRepoHint: "GitHub Repository im Format Besitzer/Name, z.B. ein Fork.",
        updateRepoOriginalHint: "Wie bisher werden Board, Seriennummer und Version an debmatic.de gemeldet.",
        updateInterval: "Prüfintervall",
        updateAuto: "Automatisch installieren",
        updateAutoHint: "Installiert neuere Versionen ohne Rückfrage. Die Platine startet dabei neu.",
        save: "Speichern",
        saveSuccess: "Einstellungen wurden erfolgreich gespeichert.",
        restartRequired: "Einige Änderungen werden erst nach einem Neustart aktiv.",
        restartNow: "Jetzt neu starten",
        restartConfirm:
          "HB-RF-ETH jetzt neu starten? Die Verbindung zur CCU wird dabei getrennt, die CCU Software muss danach neu gestartet werden.",
        saveError:
          "Beim Speichern der Einstellungen ist ein Fehler aufgetreten."
      },
      en: {
        title: "Settings",
        changePassword: "Change Password",
        repeatPassword: "Repeat Password",
        hostname: "Hostname",
        dhcp: "DHCP",
        enabled: "Enabled",
        disabled: "Disabled",
        ipAddress: "IP address",
        netmask: "Netmask",
        gateway: "Gateway",
        dns1: "Primary DNS",
        dns2: "Secondary DNS",
        timesource: "Timesource",
        ntp: "NTP",
        dcf: "DCF",
        gps: "GPS",
        ntpServer: "NTP Server",
        dcfOffset: "DCF Offset",
        gpsBaudrate: "GPS Baudrate",
        ledBrightness: "LED brightness",
        mqtt: "MQTT",
        mqttServer: "MQTT broker",
        mqttPort: "MQTT port",
        mqttUser: "MQTT user",
        mqttPassword: "MQTT password",
        mqttPasswordKeep: "Leave empty to keep the stored password.",
        mqttTopicPrefix: "MQTT topic prefix",
        mqttTopicPrefixHint: "Leave empty for hb-rf-eth/<serial>.",
        mqttInterval: "Publish interval",
        mqttHaDiscovery: "Home Assistant discovery",
        mqttCommands: "MQTT commands",
        mqttCommandsHint: "Control restart, LED brightness, network watchdog and firmware update via MQTT / Home Assistant.",
        ledBrightnessHint: "Applied immediately.",
        networkWatchdog: "Network watchdog",
        networkWatchdogHint:
          "Restarts the board when the network link, IP address or gateway (ping) is missing for the configured time. Each repeated restart waits twice as long (max. 24 hours).",
        networkWatchdogTimeout: "Restart after",
        minutes: "minutes",
        hours: "hours",
        updateRepo: "Update source (GitHub)",
        updateRepoOriginal: "Original",
        updateRepoCustom: "Own repository...",
        updateRepoHint: "GitHub repository as owner/name, e.g. a fork.",
        updateRepoOriginalHint: "As before, board, serial number and version are reported to debmatic.de.",
        updateInterval: "Check interval",
        updateAuto: "Install automatically",
        updateAutoHint: "Installs newer versions without asking. The board restarts.",
        save: "Save",
        saveSuccess: "Settings were successfully saved.",
        restartRequired: "Some changes only take effect after a restart.",
        restartNow: "Restart now",
        restartConfirm:
          "Restart the HB-RF-ETH now? This drops the connection to the CCU, the CCU software has to be restarted afterwards.",
        saveError: "An error occured while saving the settings."
      }
    }
  }
};
</script>

<style lang="css">
</style>