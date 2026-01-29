#!/bin/bash

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
NC='\033[0m' # No Color

echo ""
echo -e "${CYAN}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║        🎮 GOMOKU LAN MULTIPLAYER - SETUP 🎮        ║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════╝${NC}"
echo ""

# Compile Server
echo -e "${YELLOW}📦 Compiling Server...${NC}"
cd cpp-server && make clean && make
if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Server compilation failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Server compiled successfully${NC}"

# Compile Client
echo ""
echo -e "${YELLOW}📦 Compiling Client...${NC}"
cd ../cpp-client && make clean && make
if [ $? -ne 0 ]; then
    echo -e "${RED}❌ Client compilation failed!${NC}"
    exit 1
fi
echo -e "${GREEN}✓ Client compiled successfully${NC}"

cd ..

echo ""
echo -e "${GREEN}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${GREEN}║              ✅ BUILD SUCCESSFUL!                  ║${NC}"
echo -e "${GREEN}╚════════════════════════════════════════════════════╝${NC}"
echo ""
echo -e "${CYAN}╔════════════════════════════════════════════════════╗${NC}"
echo -e "${CYAN}║                  HOW TO PLAY                       ║${NC}"
echo -e "${CYAN}╠════════════════════════════════════════════════════╣${NC}"
echo -e "${CYAN}║${NC}                                                    ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ${YELLOW}1. Start Server (Terminal 1):${NC}                     ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}     cd cpp-server && ./gomoku_server               ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}                                                    ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ${YELLOW}2. Start Player 1 (Terminal 2):${NC}                   ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}     cd cpp-client && ./gomoku_client               ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}                                                    ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ${YELLOW}3. Start Player 2 (Terminal 3):${NC}                   ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}     cd cpp-client && ./gomoku_client               ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}                                                    ${CYAN}║${NC}"
echo -e "${CYAN}╠════════════════════════════════════════════════════╣${NC}"
echo -e "${CYAN}║${NC}            ${GREEN}FOR LAN MULTIPLAYER:${NC}                     ${CYAN}║${NC}"
echo -e "${CYAN}╠════════════════════════════════════════════════════╣${NC}"
echo -e "${CYAN}║${NC}                                                    ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ${YELLOW}On Host Machine:${NC}                                  ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}     cd cpp-server && ./gomoku_server               ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}                                                    ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ${YELLOW}On Remote Machine:${NC}                                ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}     cd cpp-client && ./gomoku_client <HOST_IP>     ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}                                                    ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ${YELLOW}Find Host IP with:${NC}                                ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}     ifconfig | grep 'inet '                        ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}                                                    ${CYAN}║${NC}"
echo -e "${CYAN}╠════════════════════════════════════════════════════╣${NC}"
echo -e "${CYAN}║${NC}                  ${GREEN}FEATURES:${NC}                          ${CYAN}║${NC}"
echo -e "${CYAN}╠════════════════════════════════════════════════════╣${NC}"
echo -e "${CYAN}║${NC}  ✓ Account Registration & Login                   ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ✓ View Online Players                            ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ✓ Send/Accept/Decline Challenges                 ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ✓ Real-time Game Play                            ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ✓ Resign & Draw Offer                            ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ✓ Request Rematch                                ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ✓ Game History & Replay                          ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ✓ ELO Rating System                              ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ✓ Persistent Data Storage                        ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ✓ Time Limit Support                             ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}  ✓ Beautiful Terminal UI                          ${CYAN}║${NC}"
echo -e "${CYAN}║${NC}                                                    ${CYAN}║${NC}"
echo -e "${CYAN}╚════════════════════════════════════════════════════╝${NC}"
echo ""
