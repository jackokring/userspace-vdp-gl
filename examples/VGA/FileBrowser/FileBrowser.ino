/*
  Created by Fabrizio Di Vittorio (fdivitto2013@gmail.com) - www.fabgl.com
  Copyright (c) 2019 Fabrizio Di Vittorio.
  All rights reserved.

  This file is part of FabGL Library.

  FabGL is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  FabGL is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with FabGL.  If not, see <http://www.gnu.org/licenses/>.
 */


/*
 * SD Card signals:
 *   MISO => GPIO 16
 *   MOSI => GPIO 17
 *   CLK  => GPIO 14
 *   CS   => GPIO 13
 *
 * To change above assignment fill other paramaters of FileBrowser::mountSDCard().
 */



#include <Preferences.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <stdio.h>

#include "fabgl.h"
#include "fabui.h"



#define FORMAT_ON_FAIL     true

#define SPIFFS_MOUNT_PATH  "/spiffs"
#define SDCARD_MOUNT_PATH  "/sdcard"


Preferences preferences;

fabgl::VGAController VGAController;
fabgl::PS2Controller PS2Controller;


fabgl::DriveType currentDriveType;
char const *     currentMountPath;


void unmountCurrent()
{
  if (currentDriveType == fabgl::DriveType::SPIFFS)
    FileBrowser::unmountSPIFFS();
  else
    FileBrowser::unmountSDCard();
}


void remountCurrent()
{
  if (currentDriveType == fabgl::DriveType::SPIFFS)
    FileBrowser::mountSPIFFS(FORMAT_ON_FAIL, SPIFFS_MOUNT_PATH);
  else {
    if (!FileBrowser::mountSDCard(FORMAT_ON_FAIL, SDCARD_MOUNT_PATH))
      selectFlash();  // fallback to spiflash in case of no SD card
  }
}


void selectFlash()
{
  unmountCurrent();
  currentDriveType = fabgl::DriveType::SPIFFS;
  currentMountPath = SPIFFS_MOUNT_PATH;
  remountCurrent();
}


void selectSDCard()
{
  unmountCurrent();
  currentDriveType = fabgl::DriveType::SDCard;
  currentMountPath = SDCARD_MOUNT_PATH;
  remountCurrent();
}


class MyApp : public uiApp {

  uiLabel *       WiFiStatusLbl;
  uiFileBrowser * fileBrowser;
  uiLabel *       freeSpaceLbl;

  void init() {
    rootWindow()->frameStyle().backgroundColor = RGB888(0, 0, 64);

    auto frame = new uiFrame(rootWindow(), "FileBrowser Example", Point(15, 10), Size(375, 275));
    frame->frameProps().hasCloseButton = false;

    // Flash / SDCard selector
    new uiLabel(frame, "Flash", Point(10, 25));
    new uiLabel(frame, "SD Card", Point(70, 25));
    auto flashRadio = new uiCheckBox(frame, Point(40, 25), Size(16, 16), uiCheckBoxKind::RadioButton);
    auto SDCardRadio = new uiCheckBox(frame, Point(114, 25), Size(16, 16), uiCheckBoxKind::RadioButton);
    flashRadio->setGroupIndex(1);
    SDCardRadio->setGroupIndex(1);
    flashRadio->onChange = [&]() {
      selectFlash();
      updateBrowser(true);
    };
    SDCardRadio->onChange = [&]() {
      selectSDCard();
      updateBrowser(true);
    };
    flashRadio->setChecked(true);

    // file browser
    fileBrowser = new uiFileBrowser(frame, Point(10, 45), Size(140, 180));
    fileBrowser->setDirectory(currentMountPath);

    // create directory button
    auto createDirBtn = new uiButton(frame, "Create Dir", Point(160, 25), Size(90, 20));
    createDirBtn->onClick = [&]() {
      constexpr int MAXSTRLEN = 16;
      char dirname[MAXSTRLEN + 1] = "";
      if (inputBox("Create Directory", "Name", dirname, MAXSTRLEN, "Create", "Cancel") == uiMessageBoxResult::Button1) {
        fileBrowser->content().makeDirectory(dirname);
        updateBrowser();
      }
    };

    // create empty file button
    auto createEmptyFile = new uiButton(frame, "Create Empty File", Point(160, 50), Size(90, 20));
    createEmptyFile->onClick = [&]() {
      constexpr int MAXSTRLEN = 16;
      char filename[MAXSTRLEN + 1] = "";
      if (inputBox("Create Empty File", "Name", filename, MAXSTRLEN, "Create", "Cancel") == uiMessageBoxResult::Button1) {
        int len = fileBrowser->content().getFullPath(filename);
        char fullpath[len];
        fileBrowser->content().getFullPath(filename, fullpath, len);
        AutoSuspendInterrupts autoInt;
        FILE * f = fopen(fullpath, "wb");
        fclose(f);
        updateBrowser();
      }
    };

    // rename button
    auto renameBtn = new uiButton(frame, "Rename", Point(160, 75), Size(90, 20));
    renameBtn->onClick = [&]() {
      int maxlen = fabgl::imax(16, strlen(fileBrowser->filename()));
      char filename[maxlen + 1];
      strcpy(filename, fileBrowser->filename());
      if (inputBox("Rename File", "New name", filename, maxlen, "Rename", "Cancel") == uiMessageBoxResult::Button1) {
        fileBrowser->content().rename(fileBrowser->filename(), filename);
        updateBrowser();
      }
    };

    // delete button
    auto deleteBtn = new uiButton(frame, "Delete", Point(160, 100), Size(90, 20));
    deleteBtn->onClick = [&]() {
      if (messageBox("Delete file/directory", "Are you sure?", "Yes", "Cancel") == uiMessageBoxResult::Button1) {
        fileBrowser->content().remove( fileBrowser->filename() );
        updateBrowser();
      }
    };

    // format button
    auto formatBtn = new uiButton(frame, "Format", Point(160, 125), Size(90, 20));
    formatBtn->onClick = [&]() {
      if (messageBox("Format sdcard", "Are you sure?", "Yes", "Cancel") == uiMessageBoxResult::Button1) {
        FileBrowser::format(currentDriveType, 0);
        unmountCurrent();
        remountCurrent();
        updateBrowser();
      }
    };

    // setup wifi button
    auto setupWifiBtn = new uiButton(frame, "Setup WiFi", Point(260, 25), Size(90, 20));
    setupWifiBtn->onClick = [=]() {
      char SSID[32] = "";
      char psw[32]  = "";
      if (inputBox("WiFi Connect", "Network Name", SSID, sizeof(SSID), "OK", "Cancel") == uiMessageBoxResult::Button1 &&
          inputBox("WiFi Connect", "Password", psw, sizeof(psw), "OK", "Cancel") == uiMessageBoxResult::Button1) {
        AutoSuspendInterrupts autoInt;
        preferences.putString("SSID", SSID);
        preferences.putString("WiFiPsw", psw);
        connectWiFi();
      }
    };

    // download file button
    auto downloadBtn = new uiButton(frame, "Download", Point(260, 50), Size(90, 20));
    downloadBtn->onClick = [=]() {
      char URL[128] = "http://";
      if (inputBox("Download File", "URL", URL, sizeof(URL), "OK", "Cancel") == uiMessageBoxResult::Button1) {
        download(URL);
        updateFreeSpaceLabel();
      }
    };

    // free space label
    freeSpaceLbl = new uiLabel(frame, "", Point(10, 235));
    updateFreeSpaceLabel();

    // wifi status label
    WiFiStatusLbl = new uiLabel(frame, "", Point(10, 255));
    connectWiFi();

    setFocusedWindow(fileBrowser);
  }

  void updateBrowser(bool goToRootDir = false)
  {
    if (goToRootDir)
      fileBrowser->setDirectory(currentMountPath);
    fileBrowser->update();
    updateFreeSpaceLabel();
  }

  // connect to wifi using SSID and PSW from Preferences
  void connectWiFi()
  {
    WiFiStatusLbl->setText("WiFi Not Connected");
    WiFiStatusLbl->labelStyle().textColor = RGB888(255, 0, 0);
    char SSID[32], psw[32];
    AutoSuspendInterrupts autoInt;
    if (preferences.getString("SSID", SSID, sizeof(SSID)) && preferences.getString("WiFiPsw", psw, sizeof(psw))) {
      WiFi.begin(SSID, psw);
      for (int i = 0; i < 16 && WiFi.status() != WL_CONNECTED; ++i) {
        WiFi.reconnect();
        delay(1000);
      }
      if (WiFi.status() == WL_CONNECTED) {
        WiFiStatusLbl->setTextFmt("Connected to %s, IP is %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
        WiFiStatusLbl->labelStyle().textColor = RGB888(0, 128, 0);
      }
    }
    WiFiStatusLbl->update();
  }

  // download from URL. Filename is the last part of the URL
  void download(char const * URL) {
    char const * slashPos = strrchr(URL, '/');
    if (slashPos) {
      char filename[slashPos - URL + 1];
      strcpy(filename, slashPos + 1);

      HTTPClient http;
      http.begin(URL);
      int httpCode = http.GET();
      if (httpCode == HTTP_CODE_OK) {

        int fullpathLen = fileBrowser->content().getFullPath(filename);
        char fullpath[fullpathLen];
        fileBrowser->content().getFullPath(filename, fullpath, fullpathLen);
        FILE * f = fopen(fullpath, "wb");

        int len = http.getSize();
        uint8_t buf[128] = { 0 };
        WiFiClient * stream = http.getStreamPtr();
        while (http.connected() && (len > 0 || len == -1)) {
          size_t size = stream->available();
          if (size) {
            int c = stream->readBytes(buf, fabgl::imin(sizeof(buf), size));
            AutoSuspendInterrupts autoInt;
            fwrite(buf, c, 1, f);
            if (len > 0)
              len -= c;
          }
        }

        AutoSuspendInterrupts autoInt;
        fclose(f);

        updateBrowser();
      }
    }
  }

  // show used and free SD space
  void updateFreeSpaceLabel() {
    int64_t total, used;
    FileBrowser::getFSInfo(currentDriveType, 0, &total, &used);
    freeSpaceLbl->setTextFmt("%lld KiB used, %lld KiB free", used / 1024, (total - used) / 1024);
    freeSpaceLbl->update();
  }

} app;


void setup()
{
  Serial.begin(115200); delay(500); Serial.write("\n\n\n"); // DEBUG ONLY

  preferences.begin("FileBrowser", false);
  preferences.clear();

  PS2Controller.begin(PS2Preset::KeyboardPort0_MousePort1, KbdMode::GenerateVirtualKeys);

  VGAController.begin();

  // maintain LOW!!! otherwise there isn't enough memory for WiFi!!!
  VGAController.setResolution(VGA_400x300_60Hz);

  // adjust this to center screen in your monitor
  //VGAController.moveScreen(-6, 0);

  Canvas cv(&VGAController);
  cv.clear();
  cv.drawText(50, 170, "Initializing SD/Flash...");
  cv.waitCompletion();
  selectFlash();
  cv.clear();
  cv.drawText(50, 170, "Connecting WiFi...");
  cv.waitCompletion();
}


void loop()
{
  app.run(&VGAController);
}






