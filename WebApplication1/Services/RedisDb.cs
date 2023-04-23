using APIServer.ModelDB;
using CloudStructures;
using CloudStructures.Structures;
using ZLogger;


namespace APIServer.Services;

public class RedisDb : IRedisDb
{
    readonly ILogger<RedisDb> _logger;

    RedisConnection _redisConn;

    public RedisDb(ILogger<RedisDb> logger, IConfiguration configuration)
    {
        _logger = logger;

        var RedisAddress = configuration.GetSection("DBConnection")["Redis"];
        var Redisconfig = new RedisConfig("basic", RedisAddress);
        _redisConn = new RedisConnection(Redisconfig);

        _logger.ZLogInformation("Redis Db Connected");
    }

    public async Task<ErrorCode> RegistUser(string email, string authToken, Int64 accountId)
    {
        var key = "UID_" + accountId;
        var user = new AuthUser
        {
            Email = email,
            AuthToken = authToken,
            AccountId = accountId,
            State = UserState.Default.ToString()
        };

        try
        {
            var redis = new RedisString<AuthUser>(_redisConn, key, LoginTimeSpan());
            if (await redis.SetAsync(user, LoginTimeSpan()) == false)
            {
                _logger.ZLogError($"[RegistUser] ErrorCode: {ErrorCode.LoginFailAddRedis}, Email: {email}");

                return ErrorCode.LoginFailAddRedis;
            }
        }
        catch (Exception ex)
        {
            _logger.ZLogError(ex, $"[RegistUser] ErrorCode: {ErrorCode.RegistUserFailException}, Email: {email}");
            return ErrorCode.RegistUserFailException;
        }

        return ErrorCode.None;
    }

    public TimeSpan LoginTimeSpan()
    {
        return TimeSpan.FromMinutes(RediskeyExpireTime.LoginKeyExpireMin);
    }
}
