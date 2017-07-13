
### Welcome!

The Mixer Unreal plugin helps you add features to your game that let the audience interact with people streaming your game!

### Getting started
Follow the Getting started guide to set up the plugin and your first interactive project.
[Getting started](https://github.com/mixer/interactive-unreal-plugin/wiki/Getting-started-using-Blueprints)

## Setup

This repository contains a [git submodule](https://git-scm.com/docs/git-submodule)
called `interactive-cpp`. This library is the core C++ SDK for Mixer Interactivity, which the Unreal plugin builds upon.

To clone the `interactive-unreal-plugin` repository and initialize the `interactive-cpp`
submodule, run the following command:

```
$ git clone --recursive https://github.com/mixer/interactive-unreal-plugin.git interactive-unreal-plugin
```

Alternatively, the submodule may be initialized independently from the clone
by running the following command:

```
$ git submodule update --init --recursive
```

The submodule points to the tip of the branch of the `interactive-cpp` repository
specified in `interactive-unreal-plugin`'s `.gitmodules` file. 


### Having Trouble?  

We want to help you fix your problem. The most efficient way to do that is to open an issue in our [issue Tracker](https://github.com/mixer/interactive-unreal-plugin/issues).  

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/). For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.
