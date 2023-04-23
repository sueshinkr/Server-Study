using APIServer.Services;

var builder = WebApplication.CreateBuilder(args);

builder.Services.AddTransient<IGameDb, GameDb>();
builder.Services.AddSingleton<IRedisDb, RedisDb>();
builder.Services.AddControllers();

var app = builder.Build();

IConfiguration configuration = app.Configuration;

app.UseRouting();
app.MapControllers();

app.Run(configuration["ServerAddress"]);