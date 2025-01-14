# AWSGameLiftExampleProject

This is an example project for building an unreal FPS with AWS gamelift

# Update 9/12/22

New way to install AWS server and client sdks with unreal: https://aws.amazon.com/blogs/gametech/how-to-integrate-the-aws-c-sdk-with-unreal-engine/

# How to build for server (OLD)

In VS 2019, make sure you have the latest .NET Framwork SDK (4.8) and all previous targeting packs
Make sure you also have cmake downloaded and added to your environment paths. In command prombt, make sure "cmake --help" works

### Environment variables Path
- C:\Program Files\CMake\bin
- C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin

***New Variable:***
- Name: VS150COMNTOOLS
- value: C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\Common7\Tools

### steps to generate aws-cpp-sdk-gamelift-server.dll and .lib files
1. [Download Amazon Gamelift Server SDK](https://aws.amazon.com/gamelift/getting-started/) (4.0.0) 
   - Unzip and rename to ServerSDK so it has a shorter path name
   - We only interested in GameLift-Cpp-ServerSDK-3.4.0 and GameLift-Unreal-plugin-3.3.1 copy those to convient location with short file path
   - (ServerSDK -> GameLift_04_06_2020 -> GameLift-SDK-Release-4.0.0)
2. Run command prompt in administration mode.  cd into GameLift-Cpp-ServerSDK file. run the next sequence of commands
```
mkdir out
cd out
cmake -G "Visual Studio 16 2019" -A x64 -DBUILD_FOR_UNREAL=1 ..
msbuild ALL_BUILD.vcxproj /p:Configuration=Release
```

### steps to build project for server
1. put .dll and .lib generated files into (C:\Users\MPLEX\Desktop\GameLift-Unreal-plugin-3.3.1\UE4.24.3\GameLiftServerSDK\ThirdParty\GameLiftServerSDK\Win64)
2. In project Plugins folder (Create one if it doesnt exist) and copy GameLiftServerSDK into it. (C:\Users\MPLEX\Desktop\GameLift-Unreal-plugin-3.3.1\UE4.24.3)
3. Make a copy of ProjectNameEditor.Target.cs and rename to ProjectNameServer.Target.cs, in source folder. Change all the "Editor" variables to "Server" in code
   - add [SupportedPlatforms(UnrealPlatformClass.Server)] above plublic class definition
4. in uproject file under "Plugins" section add "Name: "GameLiftServerSDK" and "Enabled": true
5. in .Build.cs file add "GameLiftServerSDK" to PublicDependencyModuleNames 
6. Regenerate VS project files
7. build under Development_Editor win64
   - GameLiftServerSDK should now be in Unreal plugins
8. add basic gamelift server code, and build under Development_Server win64

# Package project
https://youtu.be/PIg2q0wEPJc?t=2757
follw this before running locally or putting on aws

# How to build for client


# Upload to AWS (Fleet, Queue) 
Everything that goes in a fleet - https://youtu.be/_A4JiDY24gM?t=1004

### Need to create a IAM Policy and user. 
1. IAM -> Policies -> Create Policy
   - JSon Tab
     - ``` 
       "Version": "2012-10-17",
       "Statement": [
        {
            "Effect": "Allow",
            "Action": "gamelift:*",
            "Resource": "*"
            
        }
        ]
     ``` 
  - Name it something like 'AmazonGameLiftFullAcess' and create policy. Only attach this policy to users we trust. Full access to GameLift 

2. Create a new user for full programmatic access
   - Name it after Someone who will be needing full Programmatic access to gamelift (Can have more then one user who has full access)
   - Click Programmatic access and click next. This creates an access key ID and secret access key for AWS SDKs. 
   - attach the policy we created above and finish user. 
   - Save Access key and secret access key **DO NOT SHARE** 


### Download AWS Command Line Interface if you dont have it already. We need to log in with a user that has programatic access. 
1. https://aws.amazon.com/cli/ 
2. Open command prompt 
   - `aws configure` and enter your Aws Access key and Secret access key 
   - us-east-1 and json for the last two prompts

### Running AWS Locally 
It takes a long time to upload to AWS and to get a fleet going. We can launch locally to test.

1. Go Back into The ServerSDK file we downloaded and drag the GameLiftLocal folder to convientent location
2. Command prompt and cd into GameLiftLocal
   - ` java -jar GameLiftLocal.jar -p 9080 ` If for some reason the java command doesnt work: https://github.com/julienvollering/MIAmaxent/issues/2
   - this will start a aws cliant on port 9080.(9081 if 9080 doesnt work) We can then go on to open up our packaged build. The command prompt will hang until you close it
     - you will notice that AWS is calling our call back functions in the project!! "onReportHealth received from /127.0.0.1:60333 with health status: healthy"
     - This process is gamelift running locally, hence the name. You should notice it is running on port: 7779. It calls health reports every 60 seconds and our local server responds back
3. Open a new command prompt and cd into C:\Users\MPLEX\Documents\Unreal Projects\AWSGameLiftExampleProject\WindowsServer\GameLiftTutorial\Binaries\Win64
   - This is our generated package build executable
   - ` ./GameLiftTutorialServer.exe -log -port=7779 ` This will launch a new window, showing logs of the running server
     
4. Open a new command prompt to start running CLI commands
   - ` aws gamelift create-game-session --endpoint-url http://localhost:9081 --maximum-player-session-count 2 --fleet-id fleet-123 `


### Upload build to AWS console (will be using US-east-1

1. In order to run successfully on an EC2 instance we need visual c++ redistributable file.
   - https://support.microsoft.com/en-us/help/2977003/the-latest-supported-visual-c-downloads (get x64 if you dont have it already)
   - copy the downloaded file into the WindowsServer package build folder.

2. In notepad type out the following and name it install.bat, will automatically install and run files necassary to run games built in VS. Save this in WIndowsServer folder
  ``` 
  VC_redist.x64.exe /q
  Engine\Extras\Redist\en-us\UE4PrereqSetup_x64.exe /q
  ```
3. Upload build to aws with the following command
  - ` aws gamelift upload-build --name <your build name> --build-version <your build number> --build-root <local build path> --operating-system WINDOWS_2012 --region us-east-1 `
  - This build specific `aws gamelift upload-build --name GameLiftTutorial --build-version 1.0.0 --build-root "C:\Users\MPLEX\Documents\Unreal Projects\AWSGameLiftExampleProject\WindowsServer"  --operating-system WINDOWS_2012 --region us-east-1 `

4. If you go to aws gamelift console you will now see a build under us-east-1

### Creating a fleet
1. https://youtu.be/_A4JiDY24gM?t=1004
2. Fill out name and description. Set instance to Spot.
3. Launch path - do the folder with the server executable. Set port `7777` and `7778`.
4. EC2 port settings
Port ranges: `7777-7778`
Protocol: ***UDP***
IP Address Range: 0.0.0.0/0

###### Notes
1. Fleet type
   - On-Demand: more expensive fixed cost fleet that you can have whenever and for however long you want them.
   - Spot: cheaper, will drop with a two minute warning, less available, not a fixed price. better choice most of the time. Fleet-IQ uses a queue to decide which fleets to launch.
2. Instance Role ARN: Can be used to give this fleet access to other AWS features. ex) want server to read and write from DynamoDB, could attach the role to the fleet here

# Matchmaking 
