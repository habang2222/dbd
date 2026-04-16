using System.Text;
using Unity.Collections;
using Unity.Netcode;
using UnityEngine;

public sealed class NetworkWorldStateSyncService : MonoBehaviour
{
    private const string SnapshotMessageName = "dbd_world_snapshot";
    private const string SnapshotRequestMessageName = "dbd_world_snapshot_request";
    private const float SnapshotIntervalSeconds = 2f;
    private const int MaxSnapshotBytes = 1024 * 1024;

    public static NetworkWorldStateSyncService Instance { get; private set; }

    private float nextSnapshotAt;
    private bool handlersRegistered;
    private string lastAppliedSnapshotTime = string.Empty;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateOnSceneLoad()
    {
        if (FindFirstObjectByType<NetworkWorldStateSyncService>() != null)
        {
            return;
        }

        GameObject serviceObject = new GameObject("Network World State Sync Service");
        DontDestroyOnLoad(serviceObject);
        serviceObject.AddComponent<NetworkWorldStateSyncService>();
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

    private void OnDestroy()
    {
        UnregisterHandlers();
        if (Instance == this)
        {
            Instance = null;
        }
    }

    private void Update()
    {
        NetworkManager manager = NetworkManager.Singleton;
        if (manager == null || manager.CustomMessagingManager == null)
        {
            return;
        }

        RegisterHandlers(manager);

        if (!manager.IsServer || Time.unscaledTime < nextSnapshotAt)
        {
            return;
        }

        SendSnapshotToAll(manager);
        nextSnapshotAt = Time.unscaledTime + SnapshotIntervalSeconds;
    }

    public void RequestSnapshotFromServer()
    {
        NetworkManager manager = NetworkManager.Singleton;
        if (manager == null || manager.CustomMessagingManager == null || !manager.IsClient || manager.IsServer)
        {
            return;
        }

        using FastBufferWriter writer = new FastBufferWriter(1, Allocator.Temp);
            manager.CustomMessagingManager.SendNamedMessage(
                SnapshotRequestMessageName,
                NetworkManager.ServerClientId,
                writer,
                NetworkDelivery.ReliableFragmentedSequenced);
    }

    private void RegisterHandlers(NetworkManager manager)
    {
        if (handlersRegistered)
        {
            return;
        }

        manager.CustomMessagingManager.RegisterNamedMessageHandler(SnapshotMessageName, HandleSnapshotMessage);
        manager.CustomMessagingManager.RegisterNamedMessageHandler(SnapshotRequestMessageName, HandleSnapshotRequestMessage);
        handlersRegistered = true;

        if (manager.IsClient && !manager.IsServer)
        {
            RequestSnapshotFromServer();
        }
    }

    private void UnregisterHandlers()
    {
        NetworkManager manager = NetworkManager.Singleton;
        if (!handlersRegistered || manager == null || manager.CustomMessagingManager == null)
        {
            return;
        }

        manager.CustomMessagingManager.UnregisterNamedMessageHandler(SnapshotMessageName);
        manager.CustomMessagingManager.UnregisterNamedMessageHandler(SnapshotRequestMessageName);
        handlersRegistered = false;
    }

    private static void SendSnapshotToAll(NetworkManager manager)
    {
        ServerBackupSnapshot snapshot = ServerBackupSnapshot.Capture("network_sync");
        SendSnapshot(manager, snapshot, null);
    }

    private static void SendSnapshot(NetworkManager manager, ServerBackupSnapshot snapshot, ulong? targetClientId)
    {
        string json = JsonUtility.ToJson(snapshot, false);
        byte[] payload = Encoding.UTF8.GetBytes(json);
        if (payload.Length > MaxSnapshotBytes)
        {
            Debug.LogWarning("World snapshot is too large to sync: " + payload.Length + " bytes");
            return;
        }

        using FastBufferWriter writer = new FastBufferWriter(payload.Length + sizeof(int), Allocator.Temp);
        writer.WriteValueSafe(payload.Length);
        writer.WriteBytesSafe(payload, payload.Length);

        if (targetClientId.HasValue)
        {
            manager.CustomMessagingManager.SendNamedMessage(
                SnapshotMessageName,
                targetClientId.Value,
                writer,
                NetworkDelivery.ReliableFragmentedSequenced);
        }
        else
        {
            manager.CustomMessagingManager.SendNamedMessageToAll(
                SnapshotMessageName,
                writer,
                NetworkDelivery.ReliableFragmentedSequenced);
        }
    }

    private void HandleSnapshotMessage(ulong senderClientId, FastBufferReader reader)
    {
        NetworkManager manager = NetworkManager.Singleton;
        if (manager == null || manager.IsServer)
        {
            return;
        }

        reader.ReadValueSafe(out int length);
        if (length <= 0 || length > MaxSnapshotBytes)
        {
            Debug.LogWarning("Ignored invalid world snapshot length: " + length);
            return;
        }

        byte[] payload = new byte[length];
        reader.ReadBytesSafe(ref payload, length);
        string json = Encoding.UTF8.GetString(payload);
        ServerBackupSnapshot snapshot = JsonUtility.FromJson<ServerBackupSnapshot>(json);
        if (snapshot == null || snapshot.utcTime == lastAppliedSnapshotTime)
        {
            return;
        }

        lastAppliedSnapshotTime = snapshot.utcTime;
        ServerBackupService.ReplaceWorldFromSnapshot(snapshot);
    }

    private void HandleSnapshotRequestMessage(ulong senderClientId, FastBufferReader reader)
    {
        NetworkManager manager = NetworkManager.Singleton;
        if (manager == null || !manager.IsServer)
        {
            return;
        }

        SendSnapshot(manager, ServerBackupSnapshot.Capture("network_sync_request"), senderClientId);
    }
}
