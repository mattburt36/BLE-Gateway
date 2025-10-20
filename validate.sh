#!/bin/bash
# Validation script for modularized BLE Gateway code
# This script checks that all required files exist and have reasonable content

echo "========================================="
echo "BLE Gateway Modularization Validator"
echo "========================================="
echo ""

# Color codes
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

ERRORS=0
WARNINGS=0

# Check if we're in the right directory
if [ ! -f "BLE-WiFi-Gateway.ino" ]; then
    echo -e "${RED}ERROR: BLE-WiFi-Gateway.ino not found!${NC}"
    echo "Please run this script from the BLE-Gateway directory."
    exit 1
fi

echo "✓ Running from correct directory"
echo ""

# Required files
echo "Checking required files..."
echo "-------------------------------------------"

declare -a REQUIRED_FILES=(
    "BLE-WiFi-Gateway.ino"
    "config_manager.h"
    "wifi_manager.h"
    "ble_scanner.h"
    "ota_manager.h"
    "mqtt_handler.h"
)

for file in "${REQUIRED_FILES[@]}"; do
    if [ -f "$file" ]; then
        size=$(wc -l < "$file")
        echo -e "${GREEN}✓${NC} $file ($size lines)"
    else
        echo -e "${RED}✗${NC} $file - MISSING"
        ERRORS=$((ERRORS+1))
    fi
done

echo ""

# Check for specific content in main file
echo "Checking main file structure..."
echo "-------------------------------------------"

if grep -q "#include \"config_manager.h\"" BLE-WiFi-Gateway.ino; then
    echo -e "${GREEN}✓${NC} Includes config_manager.h"
else
    echo -e "${RED}✗${NC} Missing include for config_manager.h"
    ERRORS=$((ERRORS+1))
fi

if grep -q "#include \"wifi_manager.h\"" BLE-WiFi-Gateway.ino; then
    echo -e "${GREEN}✓${NC} Includes wifi_manager.h"
else
    echo -e "${RED}✗${NC} Missing include for wifi_manager.h"
    ERRORS=$((ERRORS+1))
fi

if grep -q "#include \"ble_scanner.h\"" BLE-WiFi-Gateway.ino; then
    echo -e "${GREEN}✓${NC} Includes ble_scanner.h"
else
    echo -e "${RED}✗${NC} Missing include for ble_scanner.h"
    ERRORS=$((ERRORS+1))
fi

if grep -q "#include \"ota_manager.h\"" BLE-WiFi-Gateway.ino; then
    echo -e "${GREEN}✓${NC} Includes ota_manager.h"
else
    echo -e "${RED}✗${NC} Missing include for ota_manager.h"
    ERRORS=$((ERRORS+1))
fi

if grep -q "#include \"mqtt_handler.h\"" BLE-WiFi-Gateway.ino; then
    echo -e "${GREEN}✓${NC} Includes mqtt_handler.h"
else
    echo -e "${RED}✗${NC} Missing include for mqtt_handler.h"
    ERRORS=$((ERRORS+1))
fi

if grep -q "void setup()" BLE-WiFi-Gateway.ino; then
    echo -e "${GREEN}✓${NC} Has setup() function"
else
    echo -e "${RED}✗${NC} Missing setup() function"
    ERRORS=$((ERRORS+1))
fi

if grep -q "void loop()" BLE-WiFi-Gateway.ino; then
    echo -e "${GREEN}✓${NC} Has loop() function"
else
    echo -e "${RED}✗${NC} Missing loop() function"
    ERRORS=$((ERRORS+1))
fi

echo ""

# Check module headers for key functions
echo "Checking module content..."
echo "-------------------------------------------"

if grep -q "void saveConfig()" config_manager.h; then
    echo -e "${GREEN}✓${NC} config_manager.h has saveConfig()"
else
    echo -e "${YELLOW}!${NC} config_manager.h missing saveConfig() - may be inline"
    WARNINGS=$((WARNINGS+1))
fi

if grep -q "bool tryWiFi()" wifi_manager.h; then
    echo -e "${GREEN}✓${NC} wifi_manager.h has tryWiFi()"
else
    echo -e "${YELLOW}!${NC} wifi_manager.h missing tryWiFi() - may be inline"
    WARNINGS=$((WARNINGS+1))
fi

if grep -q "void processAdvert" ble_scanner.h; then
    echo -e "${GREEN}✓${NC} ble_scanner.h has processAdvert()"
else
    echo -e "${YELLOW}!${NC} ble_scanner.h missing processAdvert() - may be inline"
    WARNINGS=$((WARNINGS+1))
fi

if grep -q "bool performOTAUpdate()" ota_manager.h; then
    echo -e "${GREEN}✓${NC} ota_manager.h has performOTAUpdate()"
else
    echo -e "${YELLOW}!${NC} ota_manager.h missing performOTAUpdate() - may be inline"
    WARNINGS=$((WARNINGS+1))
fi

if grep -q "void mqttCallback" mqtt_handler.h; then
    echo -e "${GREEN}✓${NC} mqtt_handler.h has mqttCallback()"
else
    echo -e "${YELLOW}!${NC} mqtt_handler.h missing mqttCallback() - may be inline"
    WARNINGS=$((WARNINGS+1))
fi

echo ""

# Check header guards
echo "Checking header guards..."
echo "-------------------------------------------"

declare -a HEADER_FILES=(
    "config_manager.h"
    "wifi_manager.h"
    "ble_scanner.h"
    "ota_manager.h"
    "mqtt_handler.h"
)

for file in "${HEADER_FILES[@]}"; do
    if grep -q "#ifndef" "$file" && grep -q "#define" "$file" && grep -q "#endif" "$file"; then
        echo -e "${GREEN}✓${NC} $file has header guards"
    else
        echo -e "${YELLOW}!${NC} $file missing proper header guards"
        WARNINGS=$((WARNINGS+1))
    fi
done

echo ""

# Check for duplicate definitions (common issue)
echo "Checking for potential issues..."
echo "-------------------------------------------"

# Count function definitions (should only be in headers, not multiple times)
saveConfig_count=$(grep -c "void saveConfig()" config_manager.h BLE-WiFi-Gateway.ino 2>/dev/null)
if [ "$saveConfig_count" -gt 1 ]; then
    echo -e "${YELLOW}!${NC} saveConfig() defined multiple times"
    WARNINGS=$((WARNINGS+1))
else
    echo -e "${GREEN}✓${NC} No duplicate saveConfig()"
fi

# Check file sizes are reasonable
main_size=$(wc -l < "BLE-WiFi-Gateway.ino")
if [ "$main_size" -gt 500 ]; then
    echo -e "${YELLOW}!${NC} Main file is large ($main_size lines) - should be <300"
    WARNINGS=$((WARNINGS+1))
else
    echo -e "${GREEN}✓${NC} Main file size reasonable ($main_size lines)"
fi

echo ""

# Summary
echo "========================================="
echo "SUMMARY"
echo "========================================="
echo ""

if [ $ERRORS -eq 0 ] && [ $WARNINGS -eq 0 ]; then
    echo -e "${GREEN}✓ All checks passed!${NC}"
    echo ""
    echo "The code structure looks good."
    echo "Next steps:"
    echo "  1. Compile in Arduino IDE"
    echo "  2. Upload to ESP32"
    echo "  3. Test functionality"
    echo ""
    exit 0
elif [ $ERRORS -eq 0 ]; then
    echo -e "${YELLOW}⚠ $WARNINGS warnings found${NC}"
    echo ""
    echo "The code should work, but review the warnings above."
    echo "Warnings are usually minor issues that won't prevent compilation."
    echo ""
    exit 0
else
    echo -e "${RED}✗ $ERRORS errors found${NC}"
    if [ $WARNINGS -gt 0 ]; then
        echo -e "${YELLOW}⚠ $WARNINGS warnings found${NC}"
    fi
    echo ""
    echo "Please fix the errors above before compiling."
    echo ""
    exit 1
fi
