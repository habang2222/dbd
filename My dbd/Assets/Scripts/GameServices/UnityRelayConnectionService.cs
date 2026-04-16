using System;
using System.Collections;
using System.IO;
using System.Threading.Tasks;
using Unity.Networking.Transport.Relay;
using Unity.Netcode;
using Unity.Netcode.Transports.UTP;
using Unity.Services.Authentication;
using Unity.Services.Core;
using Unity.Services.Relay;
using Unity.Services.Relay.Models;
using UnityEngine;

public sealed class UnityRelayConnectionService : MonoBehaviour
{
    private const string DefaultConnectionType = "dtls";
    private const int DefaultMaxConnections = 8;

    public static UnityRelayConnectionService Instance { get; private set; }
    public string JoinCode { get; private set; } = string.Empty;
    public string Status { get; private set; } = "Relay 준비 전";
    public bool IsBusy { get; private set; }
    public bool IsConnected => NetworkManager.Singleton != null
        && (NetworkManager.Singleton.IsHost || NetworkManager.Singleton.IsClient || NetworkManager.Singleton.IsServer);

    public event Action StatusChanged;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<UnityRelayConnectionService>() != null)
        {
            return;
        }

        GameObject serviceObject = new GameObject("Unity Relay Connection Service");
        DontDestroyOnLoad(serviceObject);
        serviceObject.AddComponent<UnityRelayConnectionService>();
    }

    private void Awake()
    {
        if (Instance != null && Instance != this)
        {
            Destroy(gameObject);
            return;
        }

        Instance = this;
    }

    private void Start()
    {
        if (HasCommandLineArgument("-relayHost"))
        {
            StartCoroutine(StartCommandLineRelayHost());
        }
    }

    private void OnDestroy()
    {
        if (Instance == this)
        {
            Instance = null;
        }
    }

    public async Task<string> StartHostWithRelay(int maxConnections = DefaultMaxConnections, string connectionType = DefaultConnectionType)
    {
        if (IsBusy)
        {
            return JoinCode;
        }

        IsBusy = true;
        SetStatus("Relay 호스트 생성 중");

        try
        {
            NetworkManager manager = EnsureNetworkManager();
            UnityTransport transport = EnsureUnityTransport(manager);
            await EnsureUnityServicesReady();

            Allocation allocation = await RelayService.Instance.CreateAllocationAsync(Mathf.Max(1, maxConnections));
            transport.SetRelayServerData(AllocationUtils.ToRelayServerData(allocation, connectionType));
            if (connectionType == "wss")
            {
                transport.UseWebSockets = true;
            }

            JoinCode = await RelayService.Instance.GetJoinCodeAsync(allocation.AllocationId);
            bool started = manager.StartHost();
            SetStatus(started ? "Relay Host: " + JoinCode : "Relay 호스트 시작 실패");
            return started ? JoinCode : string.Empty;
        }
        catch (Exception exception)
        {
            SetStatus("Relay 호스트 오류: " + exception.Message);
            return string.Empty;
        }
        finally
        {
            IsBusy = false;
            RaiseStatusChanged();
        }
    }

    public async Task<bool> StartClientWithRelay(string joinCode, string connectionType = DefaultConnectionType)
    {
        if (IsBusy)
        {
            return false;
        }

        joinCode = (joinCode ?? string.Empty).Trim().ToUpperInvariant();
        if (string.IsNullOrEmpty(joinCode))
        {
            SetStatus("Join Code를 입력해야 합니다");
            return false;
        }

        IsBusy = true;
        SetStatus("Relay 참가 중");

        try
        {
            NetworkManager manager = EnsureNetworkManager();
            UnityTransport transport = EnsureUnityTransport(manager);
            await EnsureUnityServicesReady();

            JoinAllocation allocation = await RelayService.Instance.JoinAllocationAsync(joinCode);
            transport.SetRelayServerData(AllocationUtils.ToRelayServerData(allocation, connectionType));
            if (connectionType == "wss")
            {
                transport.UseWebSockets = true;
            }

            JoinCode = joinCode;
            bool started = manager.StartClient();
            if (started && NetworkWorldStateSyncService.Instance != null)
            {
                NetworkWorldStateSyncService.Instance.RequestSnapshotFromServer();
            }

            SetStatus(started ? "Relay Client 연결 중: " + joinCode : "Relay 클라이언트 시작 실패");
            return started;
        }
        catch (Exception exception)
        {
            SetStatus("Relay 참가 오류: " + exception.Message);
            return false;
        }
        finally
        {
            IsBusy = false;
            RaiseStatusChanged();
        }
    }

    public void Shutdown()
    {
        NetworkManager manager = NetworkManager.Singleton;
        if (manager != null && IsConnected)
        {
            manager.Shutdown();
        }

        JoinCode = string.Empty;
        SetStatus("Relay 연결 종료");
    }

    private static async Task EnsureUnityServicesReady()
    {
        if (UnityServices.State == ServicesInitializationState.Uninitialized)
        {
            await UnityServices.InitializeAsync();
        }

        if (!AuthenticationService.Instance.IsSignedIn)
        {
            await AuthenticationService.Instance.SignInAnonymouslyAsync();
        }
    }

    private static NetworkManager EnsureNetworkManager()
    {
        NetworkManager manager = NetworkManager.Singleton;
        if (manager != null)
        {
            return manager;
        }

        GameObject networkObject = new GameObject("Network Manager");
        DontDestroyOnLoad(networkObject);
        manager = networkObject.AddComponent<NetworkManager>();
        if (manager.NetworkConfig == null)
        {
            manager.NetworkConfig = new NetworkConfig();
        }

        return manager;
    }

    private static UnityTransport EnsureUnityTransport(NetworkManager manager)
    {
        UnityTransport transport = manager.GetComponent<UnityTransport>();
        if (transport == null)
        {
            transport = manager.gameObject.AddComponent<UnityTransport>();
        }

        manager.NetworkConfig.NetworkTransport = transport;
        return transport;
    }

    private void SetStatus(string status)
    {
        Status = status;
        RaiseStatusChanged();
        Debug.Log(status);
    }

    private void RaiseStatusChanged()
    {
        StatusChanged?.Invoke();
    }

    private IEnumerator StartCommandLineRelayHost()
    {
        yield return null;
        int maxConnections = GetCommandLineInt("-relayMaxConnections", DefaultMaxConnections);
        Task<string> task = StartHostWithRelay(maxConnections);
        while (!task.IsCompleted)
        {
            yield return null;
        }

        string code = task.Result;
        if (!string.IsNullOrEmpty(code))
        {
            WriteRelayJoinCodeFile(code);
        }
    }

    private static bool HasCommandLineArgument(string argument)
    {
        string[] args = Environment.GetCommandLineArgs();
        for (int i = 0; i < args.Length; i++)
        {
            if (string.Equals(args[i], argument, StringComparison.OrdinalIgnoreCase))
            {
                return true;
            }
        }

        return false;
    }

    private static int GetCommandLineInt(string argument, int fallback)
    {
        string[] args = Environment.GetCommandLineArgs();
        for (int i = 0; i < args.Length - 1; i++)
        {
            if (string.Equals(args[i], argument, StringComparison.OrdinalIgnoreCase)
                && int.TryParse(args[i + 1], out int value))
            {
                return value;
            }
        }

        return fallback;
    }

    private static void WriteRelayJoinCodeFile(string code)
    {
        string path = Path.Combine(Application.persistentDataPath, "relay_join_code.txt");
        File.WriteAllText(path, code);
        Debug.Log("Relay join code written to " + path);
    }
}
