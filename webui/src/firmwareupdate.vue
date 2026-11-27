<template>
  <b-card
    :header="$t('title')"
    header-tag="h6"
    header-bg-variant="secondary"
    header-text-variant="white"
    class="mb-3"
  >
    <b-form @submit.stop.prevent>
      <b-form-group :label="$t('installedVersion')" label-cols-sm="4">
        <b-form-input type="text" v-model="this.$store.state.sysInfo.currentVersion" disabled></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('updateSource')" label-cols-sm="4">
        <b-form-input type="text" :value="online.repo" disabled></b-form-input>
      </b-form-group>
      <b-form-group :label="$t('latestVersion')" label-cols-sm="4">
        <b-input-group>
          <b-form-input type="text" :value="latestVersionText" disabled></b-form-input>
          <b-input-group-append>
            <b-button
              variant="outline-secondary"
              :disabled="busy"
              @click="checkClick"
            >{{ $t('checkNow') }}</b-button>
          </b-input-group-append>
        </b-input-group>
      </b-form-group>
      <b-alert variant="danger" :show="online.state == 'error' && !!online.error">{{ online.error }}</b-alert>
      <b-alert variant="warning" :show="online.available && online.state != 'installing' && online.state != 'installed'">
        <i18n path="updateAvailable">
          <template #latestVersion>
            <span>{{ online.latestVersion }}</span>
          </template>
          <template #release>
            <a :href="online.releaseUrl" class="alert-link" target="_blank" rel="noopener">{{ $t('releaseNotes') }}</a>
          </template>
        </i18n>
      </b-alert>
      <b-progress max="100" class="mb-3" height="1.5rem" v-if="online.state == 'installing'">
        <b-progress-bar :value="online.progress" :label="online.progress + ' %'" animated></b-progress-bar>
      </b-progress>
      <b-alert variant="success" :show="online.state == 'installed'">{{ $t('installDone') }}</b-alert>
      <b-form-group label-cols-sm="9" v-if="online.installable">
        <b-button
          variant="primary"
          block
          :disabled="busy || !online.available"
          @click="installClick"
        >{{ $t('install') }}</b-button>
      </b-form-group>
      <hr />
      <b-form-group :label="$t('updateFile')" label-cols-sm="4">
        <b-form-file
          v-model="file"
          accept=".bin"
          :placeholder="$t('noFileChosen')"
          :browse-text="$t('browse')"
        ></b-form-file>
      </b-form-group>
      <b-progress
        :value="this.$store.state.firmwareUpdate.progress"
        max="100"
        class="mb-3"
        v-if="this.$store.state.firmwareUpdate.progress>0"
        animated
      ></b-progress>
      <b-alert
        variant="success"
        :show="showSuccess"
        dismissible
        fade
        @dismissed="showSuccess=null"
      >
        {{ $t("uploadSuccess") }}
        <b-button
          size="sm"
          variant="success"
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
      >{{ $t("uploadError") }}</b-alert>
      <b-form-group label-cols-sm="9">
        <b-button
          variant="primary"
          block
          :disabled="file==null||this.$store.state.firmwareUpdate.progress>0"
          @click="firmwareUpdateClick"
        >{{ $t('upload') }}</b-button>
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

import { FormFilePlugin } from "bootstrap-vue/esm/components/form-file";
Vue.use(FormFilePlugin);

import { ProgressPlugin } from "bootstrap-vue/esm/components/progress";
Vue.use(ProgressPlugin);

import { CardPlugin } from "bootstrap-vue/esm/components/card";
Vue.use(CardPlugin);

import { InputGroupPlugin } from "bootstrap-vue/esm/components/input-group";
Vue.use(InputGroupPlugin);

import { ButtonPlugin } from "bootstrap-vue/esm/components/button";
Vue.use(ButtonPlugin);

import { AlertPlugin } from "bootstrap-vue/esm/components/alert";
Vue.use(AlertPlugin);

export default {
  name: "FirmwareUpdate",
  data() {
    return {
      file: null,
      showError: false,
      showSuccess: false,
      waitForRestart: false
    };
  },
  computed: {
    online() {
      return this.$store.state.onlineUpdate;
    },
    busy() {
      return ["checking", "installing", "installed"].indexOf(this.online.state) >= 0;
    },
    latestVersionText() {
      if (this.online.state == "checking") return this.$t("checking");
      if (!this.online.latestVersion || this.online.latestVersion == "n/a") return this.$t("unknown");
      return this.online.latestVersion;
    }
  },
  methods: {
    checkClick() {
      this.$store.dispatch("onlineUpdate/check");
    },
    installClick() {
      if (window.confirm(this.$t("installConfirm", { version: this.online.latestVersion }))) {
        this.$store.dispatch("onlineUpdate/install", false);
      }
    },
    restartClick() {
      if (window.confirm(this.$t("restartConfirm"))) {
        this.$store.dispatch("system/restart").catch(() => {
          this.showError = true;
        });
      }
    },
    firmwareUpdateClick(event) {
      var self = this;
      this.showError = null;
      this.showSuccess = null;
      this.$store.dispatch("firmwareUpdate/update", this.file).then(
        () => {
          self.showSuccess = true;
          self.file = null;
        },
        () => {
          self.showError = true;
        }
      );
    }
  },
  mounted() {
    this.$store.dispatch("sysInfo/update");
    this.$store.dispatch("onlineUpdate/load");
    this.pollTimer = setInterval(() => {
      // the board restarts by itself after an online update, reload once it is back
      if (this.online.state == "installed") {
        this.$store.dispatch("onlineUpdate/load").catch(() => {
          this.waitForRestart = true;
        });
        if (this.waitForRestart) {
          this.$store.dispatch("sysInfo/update").then(() => window.location.reload(), () => {});
        }
      } else {
        this.$store.dispatch("onlineUpdate/load").catch(() => {});
      }
    }, 2000);
  },
  beforeDestroy() {
    clearInterval(this.pollTimer);
  },
  i18n: {
    locale: navigator.language,
    fallbackLocale: "en",
    messages: {
      de: {
        title: "Firmware",
        installedVersion: "Installierte Version",
        updateAvailable: "Version {latestVersion} ist verfügbar ({release}).",
        updateSource: "Update Quelle",
        latestVersion: "Neueste Version",
        checkNow: "Jetzt prüfen",
        checking: "Wird geprüft...",
        unknown: "Unbekannt",
        releaseNotes: "Änderungen",
        install: "Update installieren",
        installConfirm:
          "Version {version} jetzt installieren? Die Platine startet danach neu, die CCU Software muss danach neu gestartet werden.",
        installDone: "Update installiert, die Platine startet neu...",
        restartNow: "Jetzt neu starten",
        restartConfirm:
          "HB-RF-ETH jetzt neu starten? Die Verbindung zur CCU wird dabei getrennt, die CCU Software muss danach neu gestartet werden.",
        updateFile: "Firmware Datei",
        noFileChosen: "Keine Datei ausgewählt",
        browse: "Datei auswählen",
        upload: "Hochladen",
        uploadSuccess:
          "Die Firmware wurde erfolgreich hochgeladen. Sie wird nach einem Neustart aktiv.",
        uploadError: "Es ist ein Fehler aufgetreten."
      },
      en: {
        title: "Firmware",
        installedVersion: "Installed version",
        updateAvailable: "Version {latestVersion} is available ({release}).",
        updateSource: "Update source",
        latestVersion: "Latest version",
        checkNow: "Check now",
        checking: "Checking...",
        unknown: "Unknown",
        releaseNotes: "release notes",
        install: "Install update",
        installConfirm:
          "Install version {version} now? The board restarts afterwards, the CCU software has to be restarted then.",
        installDone: "Update installed, the board is restarting...",
        restartNow: "Restart now",
        restartConfirm:
          "Restart the HB-RF-ETH now? This drops the connection to the CCU, the CCU software has to be restarted afterwards.",
        updateFile: "Firmware file",
        noFileChosen: "No file chosen",
        browse: "Browse",
        upload: "Upload",
        uploadSuccess:
          "Firmware successfully uploaded. It becomes active after a restart.",
        uploadError: "An error occured."
      }
    }
  }
};
</script>

<style lang="css">
</style>