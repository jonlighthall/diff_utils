# cpp

## WSL 1

EACCES: permission denied, rename home .vscode-server extensions ms-vscode cpptools linux

* try add polling, fail
* try chmod, fail
* try restarting

## Make

run `Makefile:Configure`
this will add settings to `.vscode/settings.json`
a launch target still needs to be set
run `Makefile:Set launch`... and select target

## SonarLint for VS Code

### Compilation Database

1. you will get the error:
   SonarLint is unable to analyze C and C++ files(s) because there is no configured compilation database
   * click on the button `Configure compile commands`
      * this will make the file `.vscode/settings.json`
      * then error
   * Ctrl-Shift-P SonarLint Configure the compilation...
3. No compilation databases were found in the workspace
   * link-> How to generate compile commands
4. Generate a Compilation Database   
   * create a file named .vscode/compile_commands.json 
   * settings
   * Makefile Tools
      add the line
      `"makefile.compileCommandsPath": ".vscode/compile_commands.json"`
      to `.vscode/settings.json`

### Node JS

* download latest package from <https://nodejs.org/en/download/>
  * Linux Binaries (x64)

* untar with `sudo tar -C /usr/local --strip-components=1 -xJf node-v20.11.1-linux-x64.tar.xz`
* then set execution directory to `/usr/local/bin/node`

### Connected Mode

#### Sonar Cloud

Ctrl-Shift P
SonarLint: Connect to SonarCloud
Configure

#### SonarQube

* install sonarqube
* run sonar console
* open webpage
