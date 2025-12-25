#!/bin/bash

# Prompt Demo for hash shell
# Shows various PS1 configurations

COLOR_CYAN='\033[0;36m'
COLOR_GREEN='\033[0;32m'
COLOR_YELLOW='\033[1;33m'
COLOR_BLUE='\033[0;34m'
COLOR_RESET='\033[0m'

echo -e "${COLOR_CYAN}=== Hash Shell Prompt (PS1) Demo ===${COLOR_RESET}\n"

echo -e "${COLOR_YELLOW}Default Prompt:${COLOR_RESET}"
echo "  set PS1='\\w\\g \\e#>\\e '"
echo -e "  ${COLOR_BLUE}/home/user/projects${COLOR_RESET} ${COLOR_GREEN}git:${COLOR_RESET}(${COLOR_CYAN}main${COLOR_RESET}) ${COLOR_BLUE}#>${COLOR_RESET}\n"

echo -e "${COLOR_YELLOW}Compact (Directory Name Only):${COLOR_RESET}"
echo "  set PS1='\\W\\g \\e#>\\e '"
echo -e "  ${COLOR_BLUE}projects${COLOR_RESET} ${COLOR_GREEN}git:${COLOR_RESET}(${COLOR_CYAN}main${COLOR_RESET}) ${COLOR_BLUE}#>${COLOR_RESET}\n"

echo -e "${COLOR_YELLOW}Classic Bash-style:${COLOR_RESET}"
echo "  set PS1='\\u@\\h:\\w\\$ '"
echo -e "  user@hostname:/home/user/projects $\n"

echo -e "${COLOR_YELLOW}Minimal:${COLOR_RESET}"
echo "  set PS1='\\W #> '"
echo -e "  projects #>\n"

echo -e "${COLOR_YELLOW}Two-Line Prompt:${COLOR_RESET}"
echo "  set PS1='\\w\\g\\n\\e#>\\e '"
echo -e "  ${COLOR_BLUE}/home/user/projects${COLOR_RESET} ${COLOR_GREEN}git:${COLOR_RESET}(${COLOR_CYAN}main${COLOR_RESET})"
echo -e "  ${COLOR_BLUE}#>${COLOR_RESET}\n"

echo -e "${COLOR_YELLOW}User and Directory:${COLOR_RESET}"
echo "  set PS1='\\u:\\W\\$ '"
echo -e "  user:projects $\n"

echo -e "${COLOR_YELLOW}Powerline-style:${COLOR_RESET}"
echo "  set PS1='\\u@\\h \\w\\g\\n\\e➜\\e '"
echo -e "  user@hostname /home/user/projects ${COLOR_GREEN}git:${COLOR_RESET}(${COLOR_CYAN}main${COLOR_RESET})"
echo -e "  ${COLOR_BLUE}➜${COLOR_RESET}\n"

echo -e "${COLOR_CYAN}=== Git Status Indicators ===${COLOR_RESET}\n"

echo -e "${COLOR_GREEN}Clean Repository:${COLOR_RESET}"
echo -e "  /home/user/projects ${COLOR_GREEN}git:${COLOR_RESET}(${COLOR_CYAN}main${COLOR_RESET}) #>\n"

echo -e "${COLOR_YELLOW}Uncommitted Changes:${COLOR_RESET}"
echo -e "  /home/user/projects ${COLOR_YELLOW}git:${COLOR_RESET}(${COLOR_CYAN}main${COLOR_RESET}) #>\n"

echo -e "No Git Repository:"
echo -e "  /home/user/projects #>\n"

echo -e "${COLOR_CYAN}=== Exit Code Indicators ===${COLOR_RESET}\n"

echo -e "${COLOR_GREEN}Success (exit code 0):${COLOR_RESET}"
echo -e "  /home/user/projects git:(main) ${COLOR_BLUE}#>${COLOR_RESET}\n"

echo -e "${COLOR_YELLOW}Failure (non-zero exit code):${COLOR_RESET}"
echo -e "  /home/user/projects git:(main) \033[0;31m#>${COLOR_RESET}\n"

echo -e "${COLOR_CYAN}=== Try These in Your .hashrc ===${COLOR_RESET}\n"

cat << 'EOF'
# Minimal
set PS1='\W #> '

# Default (recommended)
set PS1='\w\g \e#>\e '

# Compact
set PS1='\W\g \e#>\e '

# Bash-like
set PS1='\u@\h:\w\$ '

# Two-line
set PS1='\w\g\n\e#>\e '

# Power user
set PS1='\u@\h:\w\g\n\e└─$\e '
EOF

echo -e "\n${COLOR_GREEN}See docs/PROMPT.md for complete documentation${COLOR_RESET}"
