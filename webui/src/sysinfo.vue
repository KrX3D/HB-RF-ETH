<template>
  <b-card
    :header="$t('title')"
    header-tag="h6"
    header-bg-variant="secondary"
    header-text-variant="white"
    class="mb-3"
  >
    <b-alert variant="warning" :show="this.$store.state.sysInfo.ccuRestartRequired">{{ $t('ccuRestartRequired') }}</b-alert>
    <b-form @submit.stop.prevent>
      <b-form-group :label="$t('serial')" label-cols-sm="4">
        <b-form-input type="text" v-model="this.$store.state.sysInfo.serial" disabled></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('cpuUsage')" label-cols-sm="4">
        <b-progress max="100" height="2.25rem" class="form-control p-0">
          <b-progress-bar
            :value="this.$store.state.sysInfo.cpuUsage"
            :label-html="`<span class='justify-content-center d-flex position-absolute w-100 text-dark'>${this.$store.state.sysInfo.cpuUsage.toFixed(2)}%</span>`"
          ></b-progress-bar>
        </b-progress>
      </b-form-group>
      <b-form-group :label="$t('memoryUsage')" label-cols-sm="4">
        <b-progress max="100" height="2.25rem" class="form-control p-0">
          <b-progress-bar
            :value="this.$store.state.sysInfo.memoryUsage"
            :label-html="`<span class='justify-content-center d-flex position-absolute w-100 text-dark'>${this.$store.state.sysInfo.memoryUsage.toFixed(2)}%</span>`"
          ></b-progress-bar>
        </b-progress>
      </b-form-group>
      <b-form-group :label="$t('rawUartRemoteAddress')" label-cols-sm="4">
        <b-form-input type="text" v-model="this.$store.state.sysInfo.rawUartRemoteAddress" disabled></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('resetReason')" label-cols-sm="4">
        <b-form-input type="text" :value="resetReasonText" disabled></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('radioModuleType')" label-cols-sm="4">
        <b-form-input type="text" v-model="this.$store.state.sysInfo.radioModuleType" disabled></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('radioModuleSerial')" label-cols-sm="4">
        <b-form-input type="text" v-model="this.$store.state.sysInfo.radioModuleSerial" disabled></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('radioModuleBidCosRadioMAC')" label-cols-sm="4">
        <b-form-input type="text" v-model="this.$store.state.sysInfo.radioModuleBidCosRadioMAC" disabled></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('radioModuleHmIPRadioMAC')" label-cols-sm="4">
        <b-form-input type="text" v-model="this.$store.state.sysInfo.radioModuleHmIPRadioMAC" disabled></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('radioModuleSGTIN')" label-cols-sm="4">
        <b-form-input type="text" v-model="this.$store.state.sysInfo.radioModuleSGTIN" disabled></b-form-input>
      </b-form-group>
    </b-form>
  </b-card>
</template>

<script>
import Vue from "vue";

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

import { ProgressPlugin } from "bootstrap-vue/esm/components/progress";
Vue.use(ProgressPlugin);

import { CardPlugin } from "bootstrap-vue/esm/components/card";
Vue.use(CardPlugin);

import { AlertPlugin } from "bootstrap-vue/esm/components/alert";
Vue.use(AlertPlugin);

export default {
  name: "SysInfo",
  data() {
    return {};
  },
  created() {
    var self = this;
    this.updateTimer = setInterval(() => {
      self.$store.dispatch("sysInfo/update");
    }, 1000);
  },
  beforeDestroy() {
    clearInterval(this.updateTimer);
  },
  computed: {
    resetReasonText() {
      var reason = this.$store.state.sysInfo.resetReason;
      if (!reason) return "";
      // $te does not apply the locale fallback (de-DE -> de), so compare with the key instead
      var key = "resetReasons." + reason;
      var text = this.$t(key);
      return text == key ? reason : text;
    }
  },
  i18n: {
    locale: navigator.language,
    fallbackLocale: "en",
    messages: {
      de: {
        title: "Systeminformationen",
        serial: "Seriennummer",
        cpuUsage: "CPU Auslastung",
        memoryUsage: "Speicherauslastung",
        rawUartRemoteAddress: "Verbunden mit",
        radioModuleType: "Funkmodultyp",
        radioModuleSerial: "Seriennummer",
        radioModuleBidCosRadioMAC: "Funkadresse (BidCos)",
        radioModuleHmIPRadioMAC: "Funkadresse (HmIP)",
        radioModuleSGTIN: "SGTIN",
        ccuRestartRequired:
          "Die CCU versucht sich mit einer alten Verbindung neu zu verbinden (z.B. nach einem Stromausfall dieser Platine). Bitte die CCU Software neu starten.",
        resetReason: "Letzter Neustart",
        resetReasons: {
          power_on: "Einschalten / Stromausfall",
          brownout: "Unterspannung",
          task_watchdog: "Watchdog (System hing)",
          watchdog: "Watchdog",
          panic: "Absturz",
          webui: "Über die Weboberfläche",
          mqtt: "Über MQTT",
          update: "Firmware Update",
          rollback: "Update zurückgerollt",
          network_watchdog: "Netzwerk-Watchdog",
          software: "Software",
          other: "Sonstiges"
        }
      },
      en: {
        title: "System information",
        serial: "Serial number",
        cpuUsage: "CPU usage",
        memoryUsage: "Memory usage",
        rawUartRemoteAddress: "Connected with",
        radioModuleType: "Radio module type",
        radioModuleSerial: "Serial number",
        radioModuleBidCosRadioMAC: "Radio address (BidCos)",
        radioModuleHmIPRadioMAC: "Radio address (HmIP)",
        radioModuleSGTIN: "SGTIN",
        ccuRestartRequired:
          "The CCU tries to resume an old connection (e.g. after a power loss of this board). Please restart the CCU software.",
        resetReason: "Last restart",
        resetReasons: {
          power_on: "Power on / power loss",
          brownout: "Brownout",
          task_watchdog: "Watchdog (system hung)",
          watchdog: "Watchdog",
          panic: "Crash",
          webui: "Via web interface",
          mqtt: "Via MQTT",
          update: "Firmware update",
          rollback: "Update rolled back",
          network_watchdog: "Network watchdog",
          software: "Software",
          other: "Other"
        }
      }
    }
  }
};
</script>

<style lang="css">
</style>