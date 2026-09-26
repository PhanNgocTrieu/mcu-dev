#include "classifier.h"
#include "policy.h"

#include <stdio.h>
#include <string.h>

static int g_failures = 0;

static void expect(int cond, const char* msg) {
  if (!cond) {
    fprintf(stderr, "FAIL: %s\n", msg);
    g_failures++;
  } else {
    printf("OK: %s\n", msg);
  }
}

int main(void) {
  usb_device_t apple;
  memset(&apple, 0, sizeof(apple));
  apple.vendor_id = 0x05ac;
  apple.product_id = 0x12a8;
  snprintf(apple.product, sizeof(apple.product), "iPhone");
  expect(usb_classify(&apple) == USB_TYPE_IPHONE, "Apple VID -> IPhone");

  usb_device_t google;
  memset(&google, 0, sizeof(google));
  google.vendor_id = 0x18d1;
  snprintf(google.manufacturer, sizeof(google.manufacturer), "Google");
  snprintf(google.product, sizeof(google.product), "Pixel");
  expect(usb_classify(&google) == USB_TYPE_ANDROID, "Google -> Android");

  usb_device_t samsung;
  memset(&samsung, 0, sizeof(samsung));
  samsung.vendor_id = 0x04e8;
  snprintf(samsung.interfaces, sizeof(samsung.interfaces), ":ff4201:");
  expect(usb_classify(&samsung) == USB_TYPE_ANDROID, "Samsung VID -> Android");

  usb_device_t stick;
  memset(&stick, 0, sizeof(stick));
  stick.vendor_id = 0x0781;
  snprintf(stick.interfaces, sizeof(stick.interfaces), ":080650:");
  snprintf(stick.product, sizeof(stick.product), "USB Disk");
  expect(usb_classify(&stick) == USB_TYPE_MASS_STORAGE, "Mass storage class");

  usb_policy_t* policy = usb_policy_create();
  usb_device_t a = google;
  a.type = USB_TYPE_ANDROID;
  a.state = USB_STATE_READY;
  snprintf(a.device_id, sizeof(a.device_id), "usb-1-2");
  snprintf(a.serial, sizeof(a.serial), "ABC");

  usb_device_t b = apple;
  b.type = USB_TYPE_IPHONE;
  b.state = USB_STATE_READY;
  snprintf(b.device_id, sizeof(b.device_id), "usb-1-3");
  snprintf(b.serial, sizeof(b.serial), "XYZ");

  usb_policy_decision_t d;
  usb_policy_can_start(policy, &a, USB_MODE_ANDROID_AUTO, NULL, &d);
  expect(d.allowed, "AA start allowed when idle");

  usb_session_t active;
  memset(&active, 0, sizeof(active));
  snprintf(active.device_id, sizeof(active.device_id), "%s", a.device_id);
  active.mode = USB_MODE_ANDROID_AUTO;
  active.state = USB_SESSION_ACTIVE;
  usb_policy_can_start(policy, &b, USB_MODE_CARPLAY, &active, &d);
  expect(!d.allowed, "exclusive: reject CarPlay while AA active");
  expect(strstr(d.reason, "exclusive") != NULL, "exclusive reason string");

  usb_policy_set_allowlist(policy, 1);
  usb_policy_allow_serial(policy, "ABC");
  usb_policy_can_start(policy, &a, USB_MODE_ANDROID_AUTO, NULL, &d);
  expect(d.allowed, "allowlist permits ABC");
  usb_policy_can_start(policy, &b, USB_MODE_CARPLAY, NULL, &d);
  expect(!d.allowed, "allowlist rejects XYZ");
  expect(usb_policy_preferred_mode(&b) == USB_MODE_CARPLAY, "iPhone prefers CarPlay");

  usb_policy_destroy(policy);
  if (g_failures) {
    fprintf(stderr, "%d failures\n", g_failures);
    return 1;
  }
  printf("All checks passed\n");
  return 0;
}
