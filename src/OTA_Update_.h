#include <Update.h>

// void performUpdate(Stream &updateSource, size_t updateSize) {
//   if (Update.begin(updateSize)) {
//     size_t written = Update.writeStream(updateSource);
//     if (written == updateSize) {
//       Serial.println("Written : " + String(written) + " successfully");
//     }
//     else {
//       Serial.println("Written only : " + String(written) + "/" + String(updateSize) + ". Retry?");
//     }
//     msg_status = "Written : " + String(written) + "/" + String(updateSize) + " [" + String((written / updateSize) * 100) + "%] \n";
//     if (Update.end()) {
//       Serial.println("OTA done!");
//       msg_status = "OTA Done: ";
//       if (Update.isFinished()) {
//         Serial.println("Update successfully completed. Rebooting...");
//         status_update = true;
//         msg_status = "Success!\n";
//       }
//       else {
//         Serial.println("Update not finished? Something went wrong!");
//         msg_status = "Failed!\n";
//       }

//     }
//     else {
//       Serial.println("Error Occurred. Error #: " + String(Update.getError()));
//       msg_status = "Error #: " + String(Update.getError());
//     }
//   }
//   else
//   {
//     Serial.println("Not enough space to begin OTA");
//     msg_status = "Not enough space for OTA";
//   }
// }

