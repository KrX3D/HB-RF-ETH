<template>
  <b-navbar type="dark" variant="primary" toggleable="sm" class="mb-3 mt-3 rounded">
    <b-navbar-brand tag="h1" class="font-weight-bold">HB-RF-ETH</b-navbar-brand>
    <b-navbar-toggle target="nav-collapse"></b-navbar-toggle>
    <b-collapse id="nav-collapse" is-nav>
      <b-navbar-nav>
        <b-nav-item href="/">{{ $t('home') }}</b-nav-item>
        <b-nav-item href="/settings">{{ $t('settings') }}</b-nav-item>
        <b-nav-item href="/firmware">{{ $t('firmware') }}</b-nav-item>
        <b-nav-item href="/about">{{ $t('about') }}</b-nav-item>
      </b-navbar-nav>
      <b-navbar-nav v-if="$store.state.login.isLoggedIn" class="ml-auto">
        <b-nav-form form-class="flex-nowrap">
          <b-button
            size="sm"
            variant="outline-light"
            class="mr-2"
            :disabled="$store.state.system.restarting"
            @click.prevent="restart"
          >{{ $store.state.system.restarting ? $t('restarting') : $t('restart') }}</b-button>
          <b-button size="sm" variant="outline-light" @click.prevent="logout">{{ $t('logout') }}</b-button>
        </b-nav-form>
      </b-navbar-nav>
    </b-collapse>
  </b-navbar>
</template>

<script>
import Vue from "vue";

import { NavbarPlugin } from "bootstrap-vue/esm/components/navbar";
Vue.use(NavbarPlugin);

import { CollapsePlugin } from "bootstrap-vue/esm/components/collapse";
Vue.use(CollapsePlugin);

export default {
  name: "Header",
  methods: {
    restart() {
      if (window.confirm(this.$t("restartConfirm"))) {
        this.$store.dispatch("system/restart").catch(() => window.alert(this.$t("restartError")));
      }
    },
    logout() {
      this.$store.dispatch("login/logout");
      this.$router.go();
    }
  },
  i18n: {
    locale: navigator.language,
    fallbackLocale: "en",
    messages: {
      de: {
        home: "Status",
        settings: "Einstellungen",
        firmware: "Firmware",
        about: "Über",
        logout: "Abmelden",
        restart: "Neustart",
        restarting: "Startet neu...",
        restartConfirm:
          "HB-RF-ETH jetzt neu starten? Die Verbindung zur CCU wird dabei getrennt, die CCU Software muss danach neu gestartet werden.",
        restartError: "Der Neustart konnte nicht ausgelöst werden.",
      },
      en: {
        home: "Home",
        settings: "Settings",
        firmware: "Firmware",
        about: "About",
        logout: "Logout",
        restart: "Restart",
        restarting: "Restarting...",
        restartConfirm:
          "Restart the HB-RF-ETH now? This drops the connection to the CCU, the CCU software has to be restarted afterwards.",
        restartError: "The restart could not be triggered.",
      }
    }
  }
};
</script>